/**
 * @file    emu_main.c
 * @brief   PLCFX1 emulator for Windows: the BluePill PLC firmware on a PC COM port.
 *
 * The emulator runs the real firmware sources (src/plc_serial_protocol.c and the selected
 * interpreter profile) and replaces only the hardware: USART1 + DMA become a COM port
 * (one end of a virtual null-modem pair such as VSPD or com0com), flash becomes an image
 * file, X/Y become console-controlled variables, the RTC follows the PC clock.
 *
 * The main loop has the same shape as App/main.c. Interrupts are delivered between loop
 * steps, never in the middle of firmware code, which is one of the interleavings that the
 * real interrupt-driven firmware must already handle:
 *
 *   loop:  1 ms ticks (TIM2)  ->  received bytes (USART1 IRQ, replies sent at once by "DMA")
 *          ->  sample inputs  ->  one scan  ->  pending reply  ->  outputs and LEDs
 *          ->  error watch  ->  calendar registers once per second  ->  console keys
 *
 * Every error (startup, COM port, rejected requests with a decoded reason, PLC faults,
 * flash failures, crashes) is written to the log file; see emu_log.h.
 */
#include <conio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "board_clock.h"
#include "emu_board.h"
#include "emu_io.h"
#include "emu_log.h"
#include "emu_memory.h"
#include "emu_protocol.h"
#include "emu_serial.h"
#include "plc_io.h"
#include "plc_memory.h"
#include "plc_memory_map.h"
#include "plc_rtc.h"
#include "plc_runtime.h"
#include "plc_runtime_profile.h"
#include "plc_serial_protocol.h"
#include "plc_special_devices.h"

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib") /* timeBeginPeriod() */
#endif

#define EMULATOR_VERSION "1.0"

/* ---- Options ------------------------------------------------------------------------------- */

typedef struct {
  const char *port;
  unsigned long baud;
  const char *log_path;
  const char *flash_path;
  bool trace;
  bool run_at_start;
  bool factory_reset;
  bool quiet;
  bool console_keys;
  bool flash_fail;
  unsigned scan_delay_ms;
} options_t;

static options_t options = {NULL, 19200UL, "plcfx1_emulator.log", "plcfx1_flash.bin",
                            false, false, false, false, true, false, 1U};

static volatile LONG quit_requested;

/* ---- Small helpers ------------------------------------------------------------------------- */

static const char *profile_name(void) {
  return PLC_RUNTIME_EXPERIMENTAL ? "experimental (src/plc_exp_*.c)" : "minimal (src/plc_runtime_minimal.c)";
}

static const char *fault_name(plc_runtime_error code) {
  switch (code) {
    case PLC_RUNTIME_OK: return "no fault";
    case PLC_RUNTIME_OPCODE: return "OPCODE: instruction word is not supported by this runtime profile";
    case PLC_RUNTIME_OPERAND: return "OPERAND: invalid operand (wrong type, out of range or read-only target)";
    case PLC_RUNTIME_BOUNDS: return "BOUNDS: access outside the PLC image or the program area";
    case PLC_RUNTIME_STACK: return "STACK: logic-block, branch or nesting stack violation";
    case PLC_RUNTIME_NO_END: return "NO_END: the program has no END instruction";
    case PLC_RUNTIME_LEGACY_MISSING: return "LEGACY_MISSING: dispatch slot without a handler";
    case PLC_RUNTIME_LEGACY_INCOMPLETE: return "LEGACY_INCOMPLETE: handler known to be incomplete";
    case PLC_RUNTIME_LEGACY_UNSUPPORTED: return "UNSUPPORTED: instruction deliberately not supported or hardware missing";
    case PLC_RUNTIME_LEGACY_BUDGET: return "BUDGET: per-scan instruction budget exhausted (endless loop?)";
    default: return "unknown fault code";
  }
}

/** Program word at @p step, or 0xFFFF beyond the program area. */
static u16 program_word(u32 step) {
  return step < MAXSTEP ? (PLC_START_ADDR)[step] : 0xFFFFU;
}

static bool plc_running(void) {
  return PLC_Runtime_RunRequested() && (PLC_8BIT(Addr_M8) & 3U) == 1U;
}

/** Log the size of the stored program (position of the first END word). */
static void log_program_summary(void) {
  u32 step;
  for (step = 0; step < MAXSTEP; step++) {
    if (program_word(step) == 0x000FU) {
      LOG_INFO(EMU_CAT_RUNTIME, "Stored program: first END (000F) at step %lu, %lu word(s)",
               (unsigned long)step, (unsigned long)step + 1UL);
      return;
    }
  }
  LOG_WARN(EMU_CAT_RUNTIME, "Stored program has no END word (000F): RUN will fault with NO_END");
}

/* ---- Crash and shutdown handlers ------------------------------------------------------------ */

static const char *exception_name(DWORD code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned data access";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "floating-point division by zero";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer division by zero";
    case EXCEPTION_INT_OVERFLOW: return "integer overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
    default: return "unhandled exception";
  }
}

/** Last chance: write the crash to the log, save the flash image and leave. */
static LONG WINAPI crash_handler(EXCEPTION_POINTERS *info) {
  EXCEPTION_RECORD *record = info->ExceptionRecord;
  LOG_FATAL(EMU_CAT_SYSTEM, "CRASH: %s (code 0x%08lX) at address %p", exception_name(record->ExceptionCode),
            record->ExceptionCode, record->ExceptionAddress);
  if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
    ULONG_PTR target = record->ExceptionInformation[1];
    LOG_FATAL(EMU_CAT_SYSTEM, "  %s of address 0x%p%s",
              record->ExceptionInformation[0] == 1 ? "write" : record->ExceptionInformation[0] == 8 ? "execution" : "read",
              (void *)target,
              target < 0x10000U ? " (NULL pointer?)" : " (outside the emulated flash/SRAM; the firmware followed a bad address)");
  }
  LOG_FATAL(EMU_CAT_SYSTEM, "  PLC state: fault %d at step %u, M8000..M8007 = %02X", (int)PLC_Runtime_LastError(),
            (unsigned)PLC_Runtime_ErrorStep(), (unsigned)PLC_8BIT(Addr_M8));
  emu_flash_save();
  emu_log_close();
  return EXCEPTION_EXECUTE_HANDLER;
}

static void abort_handler(int signal_number) {
  (void)signal_number;
  LOG_FATAL(EMU_CAT_SYSTEM, "abort() called (failed assertion or CRT error); the emulator stops");
  emu_flash_save();
  emu_log_close();
  _exit(3);
}

#ifdef _MSC_VER
static void invalid_parameter_handler(const wchar_t *expression, const wchar_t *function,
                                      const wchar_t *file, unsigned int line, uintptr_t reserved) {
  (void)reserved;
  LOG_ERROR(EMU_CAT_SYSTEM, "C runtime reported an invalid parameter: %ls in %ls (%ls:%u)",
            expression ? expression : L"?", function ? function : L"?", file ? file : L"?", line);
}
#endif

static BOOL WINAPI console_handler(DWORD event) {
  switch (event) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      InterlockedExchange(&quit_requested, 1);
      if (event == CTRL_CLOSE_EVENT) Sleep(2000); /* give the main loop time to close the log */
      return TRUE;
    default:
      return FALSE;
  }
}

/* ---- Emulated interrupts ------------------------------------------------------------------- */

static LARGE_INTEGER qpc_frequency, qpc_last_tick;
static unsigned long long uptime_ms;

/** TIM2 update interrupt of App/main.c. */
static void tim2_interrupt(void) {
  uptime_ms++;
  if (PLC_RUN_TIME != UINT16_MAX) PLC_RUN_TIME++;
  PLC_Runtime_Tick1ms();
  X_filter();
}

/** Deliver one tick per elapsed millisecond. */
static void service_ticks(void) {
  LARGE_INTEGER now;
  long long elapsed;
  QueryPerformanceCounter(&now);
  elapsed = (now.QuadPart - qpc_last_tick.QuadPart) * 1000LL / qpc_frequency.QuadPart;
  if (elapsed <= 0) return;
  if (elapsed > 500)
    LOG_WARN(EMU_CAT_SYSTEM, "Emulator stalled for %lld ms (debugger break or heavy PC load); PLC timers jump ahead",
             elapsed);
  qpc_last_tick.QuadPart += elapsed * qpc_frequency.QuadPart / 1000LL;
  while (elapsed-- > 0) {
    tim2_interrupt();
    emu_check_primask("the 1 ms tick");
  }
}

/** Main-loop TX_Process() plus the DMA transfer and its completion interrupt. */
static void service_tx(void) {
  if (UART_SEND_READY) TX_Process();
  emu_check_primask("TX_Process()");
  if (DMA1_Channel4->CCR & DMA_CCR_EN) {
    u32 count = DMA1_Channel4->CNDTR;
    bool ok;
    if (count == 0U || count > sizeof tx_data) {
      LOG_ERROR(EMU_CAT_PROTOCOL, "DMA started with an invalid length %lu", (unsigned long)count);
      ok = false;
    } else {
      ok = emu_serial_write((const uint8_t *)tx_data, count);
      emu_proto_on_tx((const uint8_t *)tx_data, count);
    }
    DMA1_Channel4->CNDTR = 0U;
    DMA1->ISR = ok ? DMA_ISR_TCIF4 : DMA_ISR_TEIF4;
    DMA1_Channel4_IRQHandler();
    DMA1->ISR = 0U;
    emu_check_primask("the DMA interrupt");
  }
}

/** Pass received bytes to the USART1 interrupt handler, answering each completed request. */
static void service_rx(void) {
  uint8_t buffer[256];
  emu_serial_errors errors;
  int count, i;

  if (!emu_serial_is_open() && !emu_serial_try_reopen()) return;
  count = emu_serial_read(buffer, sizeof buffer, &errors);
  if (count < 0) return;
  if (errors.framing || errors.parity || errors.overrun) {
    /* A damaged byte cannot be part of a valid frame: let the firmware drop the frame. */
    USART1->SR = (errors.framing ? USART_SR_FE : 0U) | (errors.parity ? USART_SR_PE : 0U) |
                 (errors.overrun ? USART_SR_ORE : 0U);
    USART1_IRQHandler();
    USART1->SR = 0U;
    emu_proto_on_line_error();
  }
  for (i = 0; i < count; i++) {
    emu_proto_on_rx_byte(buffer[i]);
    USART1->DR = buffer[i];
    USART1->SR = USART_SR_RXNE;
    if (USART1->CR1 & USART_CR1_RXNEIE) USART1_IRQHandler();
    USART1->SR = 0U;
    emu_check_primask("the USART1 interrupt");
    service_tx();
  }
}

/* ---- Error watch ------------------------------------------------------------------------- */

/* Previous PLC state; the zero state is the boot state (STOP, no fault, no download). */
static struct {
  bool running;
  plc_runtime_error fault;
  bool m8063;
  bool updating;
  double max_scan_ms;
  unsigned long long scans;
  unsigned long long slow_scans;
  unsigned long long last_slow_report_ms;
} watch;

/** Log a latched fault with the offending program words. */
static void log_fault(plc_runtime_error code, const char *prefix) {
  u16 step = PLC_Runtime_ErrorStep();
  LOG_ERROR(EMU_CAT_RUNTIME, "%sPLC FAULT %d - %s", prefix, (int)code, fault_name(code));
  LOG_ERROR(EMU_CAT_RUNTIME,
            "  at step %u: words %04X %04X %04X (D8067=%u, D8068=%u, M8065=%u); the PLC is in STOP",
            (unsigned)step, program_word(step), program_word(step + 1U), program_word(step + 2U),
            (unsigned)D8067, (unsigned)D8068, (unsigned)(PLC_BIT_TEST(M8065) ? 1U : 0U));
}

/** Compare the PLC state with the previous loop and log every change worth knowing. */
static void watch_plc(bool retried_run, double scan_ms) {
  bool running = plc_running();
  plc_runtime_error fault = PLC_Runtime_LastError();
  bool m8063 = PLC_BIT_TEST(M8063) != 0U;
  bool updating = PLC_UPDATE_PROG != 0U;
  u16 watchdog = D8000;

  watch.scans++;
  if (scan_ms > watch.max_scan_ms) watch.max_scan_ms = scan_ms;
  if (watchdog && scan_ms > watchdog) {
    /* At most one warning per second, so a slow program cannot flood the log. */
    if (!watch.slow_scans || uptime_ms - watch.last_slow_report_ms >= 1000U) {
      LOG_WARN(EMU_CAT_RUNTIME, "Scan took %.1f ms, longer than the watchdog time D8000 = %u ms (%llu slow scan(s) so far)",
               scan_ms, (unsigned)watchdog, watch.slow_scans + 1U);
      watch.last_slow_report_ms = uptime_ms;
    }
    watch.slow_scans++;
  }
  if (fault != watch.fault) {
    if (fault != PLC_RUNTIME_OK) log_fault(fault, "");
    else LOG_INFO(EMU_CAT_RUNTIME, "Fault %d acknowledged", (int)watch.fault);
  } else if (fault != PLC_RUNTIME_OK && retried_run) {
    log_fault(fault, "RUN refused, fault again: ");
  }
  if (updating != watch.updating) {
    if (updating) LOG_INFO(EMU_CAT_FLASH, "Program download started (PLC held in STOP)");
    else {
      LOG_INFO(EMU_CAT_FLASH, "Program download finished");
      log_program_summary();
    }
  }
  if (running != watch.running)
    LOG_INFO(EMU_CAT_RUNTIME, "PLC is now %s", running ? "RUN" : "STOP");
  if (m8063 && !watch.m8063)
    LOG_ERROR(EMU_CAT_RUNTIME, "M8063 set: serial DMA transfer error or retained-data flash error");
  watch.running = running;
  watch.fault = fault;
  watch.m8063 = m8063;
  watch.updating = updating;
}

/* ---- Console ------------------------------------------------------------------------------- */

static void print_help(void) {
  printf("\nKeys:  0..7 toggle input X0..X7   r RUN   s STOP   c clear fault   i status\n"
         "       p show program start   t trace on/off   f flash-failure injection on/off\n"
         "       h help   q quit (Ctrl+C also quits)\n\n");
}

static void print_status(void) {
  u8 y = emu_io_outputs(), x = emu_io_inputs();
  plc_runtime_error fault = PLC_Runtime_LastError();
  unsigned i;

  printf("\n--- PLCFX1 emulator status -------------------------------------------\n");
  printf(" State     : %s%s\n", plc_running() ? "RUN" : "STOP", PLC_UPDATE_PROG ? " (download in progress)" : "");
  printf(" Fault     : %d %s", (int)fault, fault_name(fault));
  if (fault) printf(" (step %u)", (unsigned)PLC_Runtime_ErrorStep());
  printf("\n X0..X7    : ");
  for (i = 0; i < 8; i++) printf("%u", (x >> i) & 1U);
  printf("   Y0..Y7: ");
  for (i = 0; i < 8; i++) printf("%u", (y >> i) & 1U);
  printf("   LED RUN=%u ERR=%u\n", emu_io_run_led(), emu_io_err_led());
  printf(" M0=%u  M8000=%u M8001=%u M8013=%u  M8063=%u M8065=%u\n", PLC_BIT_TEST(0) ? 1U : 0U,
         PLC_BIT_TEST(M8000) ? 1U : 0U, PLC_BIT_TEST(M8001) ? 1U : 0U, PLC_BIT_TEST(M8013) ? 1U : 0U,
         PLC_BIT_TEST(M8063) ? 1U : 0U, PLC_BIT_TEST(M8065) ? 1U : 0U);
  printf(" Clock     : %04u-%02u-%02u %02u:%02u:%02u\n", (unsigned)D8018, (unsigned)D8017, (unsigned)D8016,
         (unsigned)D8015, (unsigned)D8014, (unsigned)D8013);
  printf(" Scans     : %llu, longest %.2f ms, over watchdog %llu\n", watch.scans, watch.max_scan_ms,
         watch.slow_scans);
  printf(" Serial    : %s, %lu request(s), %lu NAK(s)\n", emu_serial_is_open() ? options.port : "NOT OPEN",
         emu_proto_requests(), emu_proto_naks());
  printf(" Flash     : %lu block write(s)/erase(s), failure injection %s\n", emu_flash_write_count(),
         emu_flash_get_fail() ? "ON" : "off");
  printf(" Profile   : %s\n", profile_name());
  printf(" Log file  : %s\n", emu_log_path()[0] ? emu_log_path() : "(none)");
  printf("-----------------------------------------------------------------------\n\n");
}

static void print_program(void) {
  u32 step;
  printf("\nProgram words (step: word):");
  for (step = 0; step < 64U; step++) {
    u16 word = program_word(step);
    if (step % 8U == 0U) printf("\n %4lu:", (unsigned long)step);
    printf(" %04X", word);
    if (word == 0x000FU) break;
  }
  printf("\n\n");
}

static void handle_key(int key) {
  switch (key) {
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
      emu_io_toggle_input((unsigned)(key - '0'));
      break;
    case 'r': case 'R':
      if (PLC_UPDATE_PROG) {
        LOG_WARN(EMU_CAT_RUNTIME, "Local RUN refused: a program download is in progress (finish it with E42 or RUN from the host)");
      } else {
        LOG_INFO(EMU_CAT_RUNTIME, "Local RUN request (console)");
        PLC_Runtime_RequestRun(1);
      }
      break;
    case 's': case 'S':
      LOG_INFO(EMU_CAT_RUNTIME, "Local STOP request (console)");
      PLC_Runtime_RequestRun(0);
      break;
    case 'c': case 'C':
      LOG_INFO(EMU_CAT_RUNTIME, "Fault cleared from the console");
      PLC_Runtime_ClearFault();
      break;
    case 'i': case 'I': print_status(); break;
    case 'p': case 'P': print_program(); break;
    case 't': case 'T':
      emu_proto_set_trace(!emu_proto_get_trace());
      emu_log_set_file_level(emu_proto_get_trace() ? EMU_LOG_TRACE : EMU_LOG_DEBUG);
      LOG_INFO(EMU_CAT_SYSTEM, "Wire trace %s", emu_proto_get_trace() ? "ON (written to the log file)" : "OFF");
      break;
    case 'f': case 'F':
      emu_flash_set_fail(!emu_flash_get_fail());
      LOG_WARN(EMU_CAT_FLASH, "Flash failure injection %s", emu_flash_get_fail() ? "ON: every flash write/erase fails" : "OFF");
      break;
    case 'h': case 'H': case '?': print_help(); break;
    case 'q': case 'Q': case 27: InterlockedExchange(&quit_requested, 1); break;
    default: break;
  }
}

/* Keys from a pipe or file (a script drives the emulator): a reader thread fills a
 * single-producer/single-consumer ring that the main loop drains. */
static char key_ring[256];
static volatile LONG key_head, key_tail;

static DWORD WINAPI stdin_reader(LPVOID parameter) {
  HANDLE input = (HANDLE)parameter;
  char key;
  DWORD got;
  while (ReadFile(input, &key, 1, &got, NULL) && got == 1) {
    LONG head = key_head;
    if (((head + 1) & 255) == key_tail) continue; /* ring full: drop the key */
    key_ring[head] = key;
    InterlockedExchange(&key_head, (head + 1) & 255);
  }
  return 0;
}

/** Keys come from the console, or from a pipe/file when a script drives the emulator. */
static void service_console(void) {
  static int input_kind = -1; /* -1 not yet known, 0 console, 1 reader thread, 2 none */

  if (!options.console_keys) return;
  if (input_kind < 0) {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    input_kind = 2;
    if (input && input != INVALID_HANDLE_VALUE) {
      if (GetConsoleMode(input, &mode)) input_kind = 0;
      else if (CreateThread(NULL, 0, stdin_reader, input, 0, NULL)) input_kind = 1;
    }
    LOG_DEBUG(EMU_CAT_SYSTEM, "Key input: %s",
              input_kind == 0 ? "console" : input_kind == 1 ? "redirected stdin" : "none");
  }
  if (input_kind == 0) {
    while (_kbhit()) {
      int key = _getch();
      if (key == 0 || key == 0xE0) { (void)_getch(); continue; } /* function and arrow keys */
      handle_key(key);
    }
  } else if (input_kind == 1) {
    while (key_tail != key_head) {
      char key = key_ring[key_tail];
      InterlockedExchange(&key_tail, (key_tail + 1) & 255);
      if (key != '\r' && key != '\n') handle_key((unsigned char)key);
    }
  }
}

/* ---- Command line ------------------------------------------------------------------------- */

static void list_ports(void) {
  char name[16], target[512];
  int n, found = 0;
  printf("Serial ports present:\n");
  for (n = 1; n <= 255; n++) {
    snprintf(name, sizeof name, "COM%d", n);
    if (QueryDosDeviceA(name, target, sizeof target)) {
      printf("  %-7s %s\n", name, target);
      found++;
    }
  }
  if (!found) printf("  (none)\n");
}

static void usage(void) {
  printf("PLCFX1 emulator %s - BluePill FX1N-style PLC firmware on a COM port\n"
         "Runtime profile: %s\n\n"
         "Usage: PLCFX1Emulator --port COMn [options]\n"
         "  --port NAME        COM port of the emulator's end of the null-modem pair (COM5, COM12, ...)\n"
         "  --baud N           baud rate, default 19200 (the firmware uses 19200 8N1)\n"
         "  --log FILE         log file, default plcfx1_emulator.log (appended)\n"
         "  --flash FILE       program flash image, default plcfx1_flash.bin\n"
         "  --factory          erase the stored program (factory END-only program)\n"
         "  --run              request RUN at start (the board always boots in STOP)\n"
         "  --trace            write every request and reply to the log file\n"
         "  --scan-delay MS    pause between scans, default 1 (0 = full speed, 100%% CPU)\n"
         "  --quiet            console shows warnings and errors only\n"
         "  --no-keys          do not read console keys (for scripted runs)\n"
         "  --flash-fail       start with flash-failure injection on (key f toggles it)\n"
         "  --list             list the COM ports and exit\n"
         "  --help             this text\n\n"
         "Example with a VSPD/com0com pair COM5 <-> COM6:\n"
         "  PLCFX1Emulator --port COM5\n"
         "  python tools/plc_link.py --port COM6 smoke examples/always_on_m0.hex\n",
         EMULATOR_VERSION, profile_name());
}

/** Parse the command line. @return 0 to run, 1 to exit successfully, 2 on a usage error. */
static int parse_arguments(int argc, char **argv) {
  int i;
  for (i = 1; i < argc; i++) {
    const char *arg = argv[i];
    const char *value = i + 1 < argc ? argv[i + 1] : NULL;
    if (!strcmp(arg, "--help") || !strcmp(arg, "-h") || !strcmp(arg, "/?")) { usage(); return 1; }
    if (!strcmp(arg, "--list")) { list_ports(); return 1; }
    if (!strcmp(arg, "--trace")) { options.trace = true; continue; }
    if (!strcmp(arg, "--run")) { options.run_at_start = true; continue; }
    if (!strcmp(arg, "--factory")) { options.factory_reset = true; continue; }
    if (!strcmp(arg, "--quiet")) { options.quiet = true; continue; }
    if (!strcmp(arg, "--no-keys")) { options.console_keys = false; continue; }
    if (!strcmp(arg, "--flash-fail")) { options.flash_fail = true; continue; }
    if (!value) {
      fprintf(stderr, "Unknown option or missing value: %s (see --help)\n", arg);
      return 2;
    }
    if (!strcmp(arg, "--port")) options.port = value;
    else if (!strcmp(arg, "--log")) options.log_path = value;
    else if (!strcmp(arg, "--flash")) options.flash_path = value;
    else if (!strcmp(arg, "--baud")) {
      char *end;
      options.baud = strtoul(value, &end, 10);
      if (*end || options.baud < 300UL || options.baud > 921600UL) {
        fprintf(stderr, "Invalid baud rate: %s\n", value);
        return 2;
      }
    } else if (!strcmp(arg, "--scan-delay")) {
      char *end;
      unsigned long delay = strtoul(value, &end, 10);
      if (*end || delay > 1000UL) {
        fprintf(stderr, "Invalid scan delay: %s (0..1000 ms)\n", value);
        return 2;
      }
      options.scan_delay_ms = (unsigned)delay;
    } else {
      fprintf(stderr, "Unknown option: %s (see --help)\n", arg);
      return 2;
    }
    i++;
  }
  if (!options.port) {
    fprintf(stderr, "Missing --port COMn (see --help)\n\n");
    list_ports();
    return 2;
  }
  return 0;
}

/* ---- Main ---------------------------------------------------------------------------------- */

int main(int argc, char **argv) {
  int parsed = parse_arguments(argc, argv);
  unsigned long long last_rtc = 0;
  LARGE_INTEGER scan_start, scan_end;

  if (parsed) return parsed == 1 ? 0 : 2;

  if (emu_log_open(options.log_path) != 0)
    fprintf(stderr, "WARNING: cannot open the log file %s; messages go to the console only\n", options.log_path);
  emu_log_set_console_level(options.quiet ? EMU_LOG_WARN : EMU_LOG_INFO);
  emu_log_set_file_level(options.trace ? EMU_LOG_TRACE : EMU_LOG_DEBUG);
  emu_proto_set_trace(options.trace);
  SetUnhandledExceptionFilter(crash_handler);
  signal(SIGABRT, abort_handler);
#ifdef _MSC_VER
  _set_invalid_parameter_handler(invalid_parameter_handler);
#endif
  SetConsoleCtrlHandler(console_handler, TRUE);

  LOG_INFO(EMU_CAT_SYSTEM, "PLCFX1 emulator %s, runtime profile: %s, %u-bit build", EMULATOR_VERSION,
           profile_name(), (unsigned)(sizeof(void *) * 8U));
  LOG_INFO(EMU_CAT_SYSTEM, "Log file: %s", emu_log_path()[0] ? emu_log_path() : "(none)");

  if (!emu_memory_map()) {
    LOG_FATAL(EMU_CAT_SYSTEM, "The fixed PLC memory addresses are not free in this process; cannot start");
    emu_log_close();
    return 1;
  }

  /* Same boot order as App/main.c. */
  plc_memory_init();
  PLC_IO_config();
  PLC_IO_Refresh();
  RTC_Init();
  emu_flash_load(options.flash_path);
  if (options.factory_reset) {
    LOG_INFO(EMU_CAT_FLASH, "--factory: stored program erased");
    emu_flash_factory_reset();
    emu_flash_save();
  }
  log_program_summary();
  if (options.flash_fail) {
    emu_flash_set_fail(true);
    LOG_WARN(EMU_CAT_FLASH, "--flash-fail: every flash write/erase will fail");
  }
  PLC_DATA_INIT();
  PLC_Runtime_Init();
  RTC_Get();
  emu_check_primask("initialisation");

  /* USART1 as configured by App/main.c: 19200 8N1, receiver, transmitter, RX interrupt. */
  USART1->BRR = (board_apb2_hz() + options.baud / 2U) / options.baud;
  USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
  if (!emu_serial_open(options.port, options.baud))
    LOG_ERROR(EMU_CAT_SERIAL, "Running without %s; the emulator retries every second", options.port);

  if (options.run_at_start) {
    LOG_INFO(EMU_CAT_RUNTIME, "--run: RUN requested at start");
    PLC_Runtime_RequestRun(1);
  }

  timeBeginPeriod(1);
  QueryPerformanceFrequency(&qpc_frequency);
  QueryPerformanceCounter(&qpc_last_tick);
  LOG_INFO(EMU_CAT_SYSTEM, "Emulator running (PLC in STOP until RUN). Press h for help, q to quit.");
  if (options.console_keys) print_help();

  while (!quit_requested) {
    bool retried_run;
    double scan_ms;

    service_ticks();
    service_rx();
    emu_proto_flush_noise();

    PLC_IO_SampleInputs();
    retried_run = PLC_Runtime_LastError() != PLC_RUNTIME_OK && PLC_Runtime_RunRequested();
    QueryPerformanceCounter(&scan_start);
    PLC_ProInstructParse();
    QueryPerformanceCounter(&scan_end);
    emu_check_primask("the PLC scan");
    scan_ms = (double)(scan_end.QuadPart - scan_start.QuadPart) * 1000.0 / (double)qpc_frequency.QuadPart;

    service_tx();
    PLC_IO_Refresh();
    watch_plc(retried_run, scan_ms);
    emu_io_set_leds(plc_running() && PLC_Runtime_LastError() == PLC_RUNTIME_OK,
                    PLC_Runtime_LastError() != PLC_RUNTIME_OK);

    if (uptime_ms - last_rtc >= 1000U) {
      last_rtc = uptime_ms;
      RTC_Get();
    }
    service_console();
    if (options.scan_delay_ms) Sleep(options.scan_delay_ms);
  }

  LOG_INFO(EMU_CAT_SYSTEM, "Shutting down: %llu scan(s), longest scan %.2f ms", watch.scans, watch.max_scan_ms);
  timeEndPeriod(1);
  emu_serial_close();
  emu_flash_save();
  emu_log_close();
  return 0;
}
