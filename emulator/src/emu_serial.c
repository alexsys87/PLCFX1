/**
 * @file    emu_serial.c
 * @brief   Win32 COM port access for the emulator (see emu_serial.h).
 */
#include "emu_serial.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "emu_log.h"

static HANDLE port = INVALID_HANDLE_VALUE;
static char port_name[MAX_PATH];
static char device_path[MAX_PATH];
static unsigned long port_baud;
static DWORD last_reopen_attempt;
static bool open_failure_logged; /* only the first of consecutive open failures is an ERROR */

/* Open failures are errors, except repeated failures of the automatic reopen loop. */
#define OPEN_FAILED(...)                                                                   \
  do {                                                                                     \
    emu_log(open_failure_logged ? EMU_LOG_DEBUG : EMU_LOG_ERROR, EMU_CAT_SERIAL, __VA_ARGS__); \
    open_failure_logged = true;                                                            \
  } while (0)

/** Text of a Windows error code, without the trailing line break. */
static const char *error_text(DWORD code) {
  static char text[256];
  size_t length;
  if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code,
                      MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), text, sizeof text, NULL))
    snprintf(text, sizeof text, "unknown error");
  length = strlen(text);
  while (length && (text[length - 1] == '\n' || text[length - 1] == '\r' || text[length - 1] == '.'))
    text[--length] = '\0';
  return text;
}

/** "COM12" needs the "\\.\" prefix; names that already carry a path are used as is. */
static void make_device_path(const char *name) {
  if (strncmp(name, "\\\\", 2) == 0) snprintf(device_path, sizeof device_path, "%s", name);
  else snprintf(device_path, sizeof device_path, "\\\\.\\%s", name);
}

bool emu_serial_open(const char *name, unsigned long baud) {
  DCB dcb;
  COMMTIMEOUTS timeouts;

  snprintf(port_name, sizeof port_name, "%s", name);
  port_baud = baud;
  last_reopen_attempt = GetTickCount();
  make_device_path(name);
  port = CreateFileA(device_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (port == INVALID_HANDLE_VALUE) {
    DWORD code = GetLastError();
    OPEN_FAILED("Cannot open %s: Windows error %lu (%s)%s", port_name, code,
              error_text(code),
              code == ERROR_FILE_NOT_FOUND ? " - check the port name of the virtual null-modem pair"
              : code == ERROR_ACCESS_DENIED ? " - the port is already open in another program"
                                            : "");
    return false;
  }

  memset(&dcb, 0, sizeof dcb);
  dcb.DCBlength = sizeof dcb;
  if (!GetCommState(port, &dcb)) {
    OPEN_FAILED("GetCommState(%s) failed: %s", port_name, error_text(GetLastError()));
    emu_serial_close();
    return false;
  }
  dcb.BaudRate = baud;
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  dcb.fParity = FALSE;
  dcb.fOutxCtsFlow = FALSE;
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_ENABLE;
  dcb.fRtsControl = RTS_CONTROL_ENABLE;
  dcb.fDsrSensitivity = FALSE;
  dcb.fOutX = FALSE;
  dcb.fInX = FALSE;
  dcb.fErrorChar = FALSE;
  dcb.fNull = FALSE;
  dcb.fAbortOnError = FALSE;
  if (!SetCommState(port, &dcb)) {
    OPEN_FAILED("SetCommState(%s, %lu 8N1) failed: %s", port_name, baud,
              error_text(GetLastError()));
    emu_serial_close();
    return false;
  }

  /* Reads return at once with whatever has arrived; writes give up after 2 s. */
  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 10;
  timeouts.WriteTotalTimeoutConstant = 2000;
  if (!SetCommTimeouts(port, &timeouts)) {
    OPEN_FAILED("SetCommTimeouts(%s) failed: %s", port_name,
              error_text(GetLastError()));
    emu_serial_close();
    return false;
  }
  SetupComm(port, 4096, 4096);
  PurgeComm(port, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
  open_failure_logged = false;
  LOG_INFO(EMU_CAT_SERIAL, "%s open: %lu baud, 8 data bits, no parity, 1 stop bit, no flow control",
           port_name, baud);
  return true;
}

void emu_serial_close(void) {
  if (port != INVALID_HANDLE_VALUE) {
    CloseHandle(port);
    port = INVALID_HANDLE_VALUE;
  }
}

bool emu_serial_is_open(void) { return port != INVALID_HANDLE_VALUE; }

/** Fetch and log the line errors that the driver collected. */
static void collect_errors(emu_serial_errors *errors) {
  DWORD flags = 0;
  COMSTAT status;
  if (errors) memset(errors, 0, sizeof *errors);
  if (!ClearCommError(port, &flags, &status) || !flags) return;
  if (flags & CE_FRAME) LOG_ERROR(EMU_CAT_SERIAL, "%s: framing error (wrong baud rate or noise)", port_name);
  if (flags & CE_RXPARITY) LOG_ERROR(EMU_CAT_SERIAL, "%s: parity error (the PLC expects 8N1)", port_name);
  if (flags & CE_OVERRUN) LOG_ERROR(EMU_CAT_SERIAL, "%s: receive overrun, bytes were lost", port_name);
  if (flags & CE_RXOVER) LOG_ERROR(EMU_CAT_SERIAL, "%s: receive buffer overflow, bytes were lost", port_name);
  if (flags & CE_BREAK) LOG_WARN(EMU_CAT_SERIAL, "%s: break condition received", port_name);
  if (errors) {
    errors->framing = (flags & CE_FRAME) != 0;
    errors->parity = (flags & CE_RXPARITY) != 0;
    errors->overrun = (flags & (CE_OVERRUN | CE_RXOVER)) != 0;
    errors->brk = (flags & CE_BREAK) != 0;
  }
}

int emu_serial_read(uint8_t *buffer, size_t size, emu_serial_errors *errors) {
  DWORD got = 0;
  if (port == INVALID_HANDLE_VALUE) return -1;
  collect_errors(errors);
  if (!ReadFile(port, buffer, (DWORD)size, &got, NULL)) {
    DWORD code = GetLastError();
    LOG_ERROR(EMU_CAT_SERIAL, "Read from %s failed: Windows error %lu (%s)", port_name, code,
              error_text(code));
    emu_serial_close();
    return -1;
  }
  return (int)got;
}

bool emu_serial_write(const uint8_t *data, size_t length) {
  DWORD written = 0;
  if (port == INVALID_HANDLE_VALUE) {
    LOG_ERROR(EMU_CAT_SERIAL, "Reply of %u byte(s) lost: %s is not open", (unsigned)length, port_name);
    return false;
  }
  if (!WriteFile(port, data, (DWORD)length, &written, NULL)) {
    DWORD code = GetLastError();
    LOG_ERROR(EMU_CAT_SERIAL, "Write to %s failed: Windows error %lu (%s)", port_name, code,
              error_text(code));
    emu_serial_close();
    return false;
  }
  if (written != length) {
    LOG_ERROR(EMU_CAT_SERIAL,
              "Write time-out on %s: %lu of %u byte(s) sent (is the other end of the null-modem "
              "pair open?)",
              port_name, written, (unsigned)length);
    return false;
  }
  return true;
}

bool emu_serial_try_reopen(void) {
  if (port != INVALID_HANDLE_VALUE) return true;
  if (GetTickCount() - last_reopen_attempt < 1000U) return false;
  LOG_DEBUG(EMU_CAT_SERIAL, "Trying to reopen %s", port_name);
  return emu_serial_open(port_name, port_baud);
}
