/**
 * @file test_protocol.c
 * @brief Host test of the serial programming protocol (src/plc_serial_protocol.c).
 *
 * The protocol source is #included, so the tests see its static state (the staging buffer
 * prog_write_buffer, the block bookkeeping bctrl1, rx_count, tx_count, ...). The device is
 * replaced by host memory: the PLC image, the program flash and the peripheral register
 * blocks (USART1, DMA1) are mapped at their real addresses, and the flash driver is a stub
 * that can be told to fail. Frames are fed byte by byte through RX_Process() exactly as the
 * USART interrupt does it, and the replies are checked in tx_data[]. tools/test_protocol.py
 * builds the test with AddressSanitizer and UBSan.
 *
 * Two builds share this file:
 *   - transport only (default): the runtime is mocked. A RUN/STOP request posted by the
 *     protocol is applied to the M8 byte by byte(), as the main loop does after the
 *     interrupt returned.
 *   - TEST_WITH_RUNTIME: the real runtime is linked in (the minimal one, or the experimental
 *     modules when PLC_RUNTIME_EXPERIMENTAL is 1) and integration_tests() downloads a program
 *     over the wire and runs it.
 *
 * Command line: without an argument the transport tests run; --integration (runtime build
 * only) runs integration_tests(); --wire turns the binary into a pipe-driven PLC (bytes on
 * stdin are USART input, replies go to stdout) for tools/tests/protocol/test_link.py.
 *
 * Notation used in the comments: packet('E', "7250E") is the frame STX 'E' 7 2 5 0 E ETX cs cs.
 * In the extended commands the first payload digits are the sub-command (E00 reads RAM, E10
 * writes RAM, E11 writes the program, E42 flushes, ...); addresses are sent high byte first,
 * except in the bit-forcing commands E7/E8, whose 16-bit bit address is sent low byte first.
 * The command list is at the top of plc_serial_protocol.c.
 */
#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include "../../../src/plc_serial_protocol.c"

#ifdef TEST_WITH_RUNTIME
/* Integration build: the real runtime serves the protocol code. plc_runtime_minimal.c compiles to
 * nothing when PLC_RUNTIME_EXPERIMENTAL is 1; the experimental modules are then linked as
 * separate units by tools/test_protocol.py. */
#include "../../../src/plc_runtime_minimal.c"
#if PLC_RUNTIME_EXPERIMENTAL
/* Stand-ins for firmware objects outside the tested modules (App/main.c, plc_io.c, plc_rtc.c):
 * this fixture has no free-running clock, input filter or real-time clock. */
volatile u16 PLC_RUN_TIME;
u16 X_DIY;
void RTC_Set(u16 year,u8 month,u8 day,u8 hour,u8 minute,u8 second) {
  (void)year;(void)month;(void)day;(void)hour;(void)minute;(void)second;
}
#endif
#else
/* Transport-only build: the minimum the protocol needs from the runtime - the update flag and
 * the RUN/STOP request, which byte() applies to the M8 byte. */
volatile u8 PLC_NEW_PROG;
static int requested_run = -1;
void PLC_Runtime_RequestRun(u8 run) { requested_run = run != 0; }
#endif
static unsigned writes, erased_pages, cases;  /* block writes, erased pages, frames sent */
static bool fail_flash;  /* make the flash stubs report a failure */
/** Stand-in for plc_memory.c: does [offset, offset + length) lie inside the PLC image? */
bool plc_ram_range(uint32_t offset, uint32_t length) {
  return offset <= PLC_RAM_SIZE && length <= PLC_RAM_SIZE - offset;
}
/** Stand-in for board_clock.c: the APB2 clock that the USART1 baud rate is derived from (72 MHz). */
u32 board_apb2_hz(void) { return 72000000U; }
/* Transport-only build: the bit accessors of the runtime as bounds-checked stand-ins. */
#ifndef TEST_WITH_RUNTIME
u32 PLC_BIT_TEST(u32 bit) {
  assert(bit < PLC_RAM_SIZE * 8U);
  return PLC_8BIT((bit >> 3)) & (1U << (bit & 7U));
}
void PLC_BIT_ON(u32 bit) {
  assert(bit < PLC_RAM_SIZE * 8U);
  PLC_8BIT((bit >> 3)) |= 1U << (bit & 7U);
}
void PLC_BIT_OFF(u32 bit) {
  assert(bit < PLC_RAM_SIZE * 8U);
  PLC_8BIT((bit >> 3)) &= ~(1U << (bit & 7U));
}
#endif
/**
 * Flash driver stub: store one block into the mapped flash area, or fail when fail_flash is set.
 * A write erases the block first, which counts as two 1 KB pages. The asserts check the
 * driver's own rules: an aligned block address inside the flash.
 */
bool plc_flash_write_block(uint32_t address, const uint16_t *data) {
  assert(address >= ROM_ADDR && address <= PLC_FLASH_END - PLC_FLASH_BLOCK_SIZE);
  assert(!(address & (PLC_FLASH_BLOCK_SIZE - 1U)));
  if (fail_flash) return false;
  writes++; erased_pages += 2;
  memcpy((void *)(uintptr_t)address, data, PLC_FLASH_BLOCK_SIZE);
  return true;
}
/**
 * Flash driver stub: erase one program block (fill it with 0xFF), or fail when fail_flash is set.
 * The retained block must not be erased through this call.
 */
bool plc_flash_erase_block(uint32_t address) {
  assert(address >= ROM_ADDR && address < PLC_RETAIN_BASE);
  assert(!(address & (PLC_FLASH_BLOCK_SIZE - 1U)));
  if (fail_flash) return false;
  erased_pages += 2;
  memset((void *)(uintptr_t)address, 255, PLC_FLASH_BLOCK_SIZE);
  return true;
}
/**
 * Map a zero-filled region at a fixed address. MAP_FIXED_NOREPLACE turns an address clash into
 * an error instead of silently replacing another mapping.
 */
static void map_region(uintptr_t address, size_t length) {
  assert(mmap((void *)address, length, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == (void *)address);
}
/**
 * Return to the power-on state before a group of tests: zeroed PLC image and peripheral
 * registers, erased flash that holds only the factory parameter header, idle protocol state,
 * cleared counters, and PLC_DATA_INIT() (boot in STOP). The integration build initialises
 * the runtime as well.
 */
static void reset(void) {
  memset((void *)RAM_ADDR, 0, PLC_RAM_SIZE);
  memset((void *)0x08000000UL, 255, 0x10000U);
  memcpy((void *)ROM_ADDR, PLC_ROM, sizeof(PLC_ROM)); /* Factory .plc_rom image. */
  memset((void *)0x40000000UL, 0, 0x24000U);  /* peripheral registers (USART1, DMA1) */
  memset(tx_data, 0xAB, sizeof(tx_data));  /* non-zero filler: stale reply bytes would show */
  memset(rx_data, 0, sizeof(rx_data));
  rx_count = tx_count = 0; UART_Sended = 1; UART_SEND_READY = 0;  /* idle receiver and transmitter */
  staging_busy = 0;
  writes = erased_pages = 0; fail_flash = false;
  PLC_UPDATE_PROG = PLC_NEW_PROG = 0;
  PLC_DATA_INIT();
#ifdef TEST_WITH_RUNTIME
  PLC_Runtime_Init();
#else
  requested_run = -1;
#endif
}
/**
 * Feed one received byte to the protocol, as the USART1 interrupt would. In the transport-only
 * build a RUN/STOP request posted by the protocol is applied to M8000/M8001 here (M8 byte 9 = RUN,
 * 10 = STOP), which models the main loop running after the interrupt has returned.
 */
static void byte(u8 value) {
  USART1->DR = value; RX_Process();
#ifndef TEST_WITH_RUNTIME
  /* Model the main-loop boundary, outside RX_Process, in transport-only tests. */
  if (requested_run >= 0) {
    PLC_8BIT(Addr_M8) = requested_run ? 9 : 10;
    requested_run = -1;
  }
#endif
}
/** Simulate the DMA channel 4 transfer-complete interrupt that ends a reply transmission. */
static void complete(void) {
  DMA1->ISR = DMA_ISR_TCIF4; DMA1_Channel4_IRQHandler();
  DMA1->ISR = 0;
}
/**
 * Let the main loop and the DMA finish the pending reply: start the transfer if a reply is
 * waiting, then complete it. Afterwards the transmitter is idle and new frames are accepted.
 */
static void release(void) {
  if (UART_SEND_READY) TX_Process();
  if (!UART_Sended) complete();
}
/**
 * Send one framed request: STX, the command character, the payload (ASCII hex digits), ETX and
 * two checksum digits. The checksum is the low byte of the sum of everything between STX and
 * the checksum digits (command, payload and ETX). The receiver stores the STX at index 2 of
 * rx_data, so rx_count equals the wire length + 1 after a frame.
 */
static void packet(char command, const char *hex) {
  unsigned sum = (u8)command + 3U;
  byte(2); byte(command);
  for (size_t i = 0; hex[i]; i++) { sum += (u8)hex[i]; byte(hex[i]); }
  byte(3); byte(ASCII[(sum & 255U) >> 4]); byte(ASCII[sum & 15U]);
  cases++;
}
/** Expect a single ACK as the reply, then release the transmitter. */
static void ack(void) { assert((u8)tx_data[0] == 6 && tx_count == 1); release(); }
/**
 * Expect a single NAK as the reply, then release the transmitter. A mismatch is reported with the
 * number of the failing case before the assert fires.
 */
static void nak(void) {
  if ((u8)tx_data[0] != 21 || tx_count != 1)
    fprintf(stderr, "NAK expected in case %u, got byte %02X count %u\n", cases, (u8)tx_data[0], tx_count);
  assert((u8)tx_data[0] == 21 && tx_count == 1); release();
}
/**
 * Expect a data frame whose payload is @p hex, with correct ETX and checksum, then release the
 * transmitter.
 */
static void response(const char *hex) {
  unsigned sum = 3U; size_t n = strlen(hex);
  if (tx_count != n + 4U || memcmp(tx_data + 1, hex, n))
    fprintf(stderr, "Response case %u: expected %s, got %.*s (count %u)\n", cases, hex, (int)n, tx_data + 1, tx_count);
  assert(tx_count == n + 4U && tx_data[0] == 2);
  assert(!memcmp(tx_data + 1, hex, n) && tx_data[n + 1U] == 3);
  for (size_t i = 0; i < n; i++) sum += (u8)hex[i];
  assert(tx_data[n + 2U] == ASCII[(sum & 255U) >> 4]);
  assert(tx_data[n + 3U] == ASCII[sum & 15U]); release();
}
/**
 * Bit forcing and the RUN/STOP bits. E7/E8 carry the bit address low byte first ("78000" is E7,
 * bit 0x0080), the commands 7 and 8 carry it high byte first. M8036 (0x0E24) requests RUN and
 * M8037 (0x0E25) requests STOP; neither bit is set directly.
 */
static void force_tests(void) {
  reset();
  assert(PLC_8BIT(Addr_M8) == 10); /* Boot STOP. */
  packet('E', "78000"); ack(); assert(PLC_BIT_TEST(0x80) && PLC_8BIT(Addr_M8) == 10);  /* E7: set bit 0x0080 */
  packet('E', "88000"); ack(); assert(!PLC_BIT_TEST(0x80));  /* E8: reset it */
  packet('E', "7250E"); ack(); assert(PLC_8BIT(Addr_M8) == 10);  /* E7 on M8037 (0x0E25): STOP request */
  /* M8036 (0x0E24) requests RUN and ends the update state. */
  PLC_UPDATE_PROG = 1;
  packet('E', "7240E"); ack(); assert(PLC_8BIT(Addr_M8) == 9 && !PLC_UPDATE_PROG);
  packet('E', "7230E"); ack();  /* M8035 (0x0E23) is ignored on E7, but acknowledged */
  packet('E', "7G000"); nak();  /* non-hexadecimal digit */
  packet('E', "7FFFF"); ack(); assert(PLC_BIT_TEST(0xFFFF));  /* the highest 16-bit bit address */
  /* Commands 7 and 8 do the same for ordinary bits, with the address high byte first. */
  packet('7', "0080"); ack(); assert(PLC_BIT_TEST(0x80));
  packet('8', "0080"); ack(); assert(!PLC_BIT_TEST(0x80));
}
/**
 * Addresses, lengths and malformed frames of the memory commands: RAM read/write (0, 1, E00, E10),
 * the size limits of a frame, the clear commands and frames that must be refused.
 */
static void bounds_tests(void) {
  reset();
  packet('1', "00800280FF"); ack();  /* write 80 FF at 0x0080 */
  assert(PLC_8BIT(0x80) == 0x80 && PLC_8BIT(0x81) == 255);
  packet('0', "008002"); response("80FF");  /* read the two bytes back */
  /* E10 at 0x4080: D0000 is at 0x4000 on the host side, so this is RAM offset 0x2080. */
  packet('E', "10408002ABCD"); ack();
  packet('E', "00408002"); response("ABCD");  /* E00 reads them back */
  /* The last byte of the PLC image can be accessed, one byte beyond it cannot. */
  packet('1', "2FFF01FE"); ack();
  packet('0', "2FFF01"); response("FE");
  packet('0', "2FFF02"); nak();  /* read beyond the image */
  packet('1', "2FFF02AABB"); nak();  /* write beyond the image */
  /* The longest reply holds 70 data bytes (a 144-byte frame); 71 bytes do not fit. */
  packet('0', "000046"); assert(tx_count == 144); release();
  packet('0', "000047"); nak();  /* 71 bytes: too long */
  /* The longest write: 65 data bytes (0xAA) at 0x0100. */
  char maximum[137];
  strcpy(maximum, "010041");
  memset(maximum + 6, 'A', 130); maximum[136] = 0;
  packet('1', maximum); ack();
  for (unsigned i = 0; i < 65; i++) assert(PLC_8BIT(0x100 + i) == 0xAA);
  packet('0', "800001"); nak(); /* high byte must not sign-extend */
  packet('7', "6000"); ack(); assert(PLC_BIT_TEST(0x6000));  /* a bit address inside the image */
  packet('E', "10408002AA"); nak(); /* missing final data byte */
  packet('1', "008002AA"); nak();  /* the same through command 1 */
  packet('1', "008001aF"); ack(); assert(PLC_8BIT(0x80) == 0xAF);  /* lower-case digits are accepted */
  /* Every command is refused when its payload is empty or too short. */
  const char commands[] = "0178E";
  for (unsigned i = 0; i < sizeof(commands) - 1U; i++) {
    packet(commands[i], ""); nak();
    packet(commands[i], "00"); nak();
  }
  /* Extended commands with a missing payload, a missing data byte, an unknown sub-command and an odd
   * number of digits. */
  packet('E', "00"); nak(); packet('E', "41"); nak();
  packet('E', "11008001"); nak();
  packet('E', "ZZ"); nak(); packet('0', "00000"); nak();
  /* Short, checksum-correct commands must not reuse the last response. */
  PLC_8BIT(0x17F) = 0xFF;
  packet('E', "62"); ack(); assert(PLC_8BIT(0x17F) == 0);  /* E62 clears M0..M3071 */
  packet('E', "63"); ack(); packet('E', "64"); ack();  /* E63 clears D0..D1999, E64 clears D0..D999 */
  packet('4', ""); ack(); packet('B', ""); ack();  /* legacy handshake and flush, no payload */
  /* Frames assembled byte by byte to provoke framing and checksum errors. */
  byte(2); byte('0'); byte('0'); byte(3); byte('G'); byte('G'); nak();
  byte(2); byte('0'); byte(3); byte('0'); byte('0'); nak();
  byte(2); byte('0'); byte('0'); byte('0'); byte('0'); byte('0'); byte('0'); byte('1');
  byte(3); byte('0'); byte('0'); nak(); /* valid hex, incorrect checksum */
  byte(2); byte('1'); byte('0'); assert(rx_count > 0 && !UART_SEND_READY);  /* incomplete frame: no reply */
  packet('0', "008001"); response("00"); /* new STX resynchronizes */
  byte(2); for (unsigned i = 0; i < 142; i++) byte('A'); nak();  /* an overlong frame is refused */
}
/**
 * Program download through the flash staging buffer (E11 at 0x8000 + offset, E42, B, E60, E61).
 * Program words are collected in prog_write_buffer; a block is written to flash when the download
 * moves to another block or when the host flushes. PLC_ROM_16BIT(offset) reads the program flash.
 */
static void flash_tests(void) {
  reset();
  memset((void *)ROM_ADDR, 0x5A, PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE);
  packet('E', "1180800280FF"); ack();  /* stage the word 0xFF80 at offset 0x80 */
  assert(!writes && prog_write_buffer[0x80 / 2] == 0xFF80);  /* staged only, the flash is untouched */
  /* Four bytes at offset 0x7FE: the first word ends block 0, the second starts block 1, so block 0
   * is written to flash. */
  packet('E', "1187FE0411223344"); ack(); /* crosses 2 KB boundary */
  assert(writes == 1 && PLC_ROM_16BIT(0x80) == 0xFF80);
  assert(PLC_ROM_16BIT(0x7FE) == 0x2211 && PLC_ROM_16BIT(0x800) == 0x5A5A);
  packet('E', "42"); ack();  /* E42 writes the staged block 1 */
  assert(writes == 2 && PLC_ROM_16BIT(0x800) == 0x4433);
  assert(PLC_ROM_16BIT(0x82) == 0x5A5A && PLC_ROM_16BIT(0x802) == 0x5A5A);  /* neighbouring words are preserved */
  /* Command B flushes the staged block like E42 does. */
  packet('E', "11808202ABCD"); ack(); packet('B', ""); ack();
  assert(PLC_ROM_16BIT(0x82) == 0xCDAB);
  packet('E', "118081021122"); nak(); /* unaligned */
  packet('E', "1180800111"); nak(); /* odd length */
  /* A write that runs past the end of the 14 KB program area is refused. */
  packet('E', "11B7FE0411223344"); nak();
  /* E01 reads program memory (the 0x8000 flag is optional). The last byte of the program area can be
   * read, one byte beyond it cannot. */
  packet('E', "01B7FF01"); response("5A");
  packet('E', "0137FF01"); response("5A");
  packet('E', "01B7FF02"); nak();
  packet('E', "01B80001"); nak();
  /* A failing flash write is answered with NAK and keeps the block staged, so the host can repeat E42. */
  packet('E', "118100023412"); ack();
  fail_flash = true; packet('E', "42"); nak(); assert(bctrl1 == 0);
  fail_flash = false; packet('E', "42"); ack(); assert(PLC_ROM_16BIT(0x100) == 0x1234);
  /* E61 discards the staged block without writing it. */
  packet('E', "118102024321"); ack(); packet('E', "61"); ack();
  packet('B', ""); ack(); assert(PLC_ROM_16BIT(0x102) == 0x5A5A);
  /* E60 erases the program area: the parameter header in front of START_ADDR survives, the first
   * program word becomes END (0x000F) and the retained block is left alone. */
  unsigned pages = erased_pages;
  packet('E', "60"); ack();
  assert(erased_pages - pages == PLC_PROGRAM_BLOCKS * 2U);
  assert(PLC_ROM_16BIT(0x5A) == 0x5A5A && PLC_ROM_16BIT(0x5C) == 15);
  for (u32 i = 0x5E; i < PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE; i++)
    assert(PLC_ROM_8BIT(i) == 255);
  assert(*(u16 *)PLC_RETAIN_BASE == 0xFFFF);
  /* E11 below 0x8000 writes RAM bytes, but it still stops the PLC and flags a program update. */
  packet('E', "11008002AABB"); ack();
  assert(PLC_8BIT(0x80) == 0xAA && PLC_UPDATE_PROG && PLC_8BIT(Addr_M8) == 10);
}
/**
 * E41 searches the program area for a word. The reply is '1' and the program-space address of
 * the first match (0x8000 + byte offset).
 */
static void search_tests(void) {
  reset();
  PLC_ROM_16BIT(0x100) = 0xFF80;  /* the word to find sits at offset 0x100 */
  packet('E', "41800080FF");  /* search from 0x8000 for 0xFF80 (low byte 80, high byte FF) */
  assert(tx_count == 9 && !memcmp(tx_data + 1, "18100", 5));  /* reply: '1' and 0x8100 */
  /* Check the checksum of the 9-byte reply. */
  unsigned sum = 3; for (unsigned i = 1; i <= 5; i++) sum += tx_data[i];
  assert(tx_data[7] == ASCII[(sum & 255) >> 4] && tx_data[8] == ASCII[sum & 15]); release();
  /* Not found: the reply holds the address just past the program area. A start address without
   * the 0x8000 flag is invalid and gives FFFF. */
  packet('E', "41B7FE80FF"); assert(!memcmp(tx_data + 2, "B800", 4)); release();
  packet('E', "417FFF80FF"); assert(!memcmp(tx_data + 2, "FFFF", 4)); release();
}
/**
 * Retained data. PLC_DATA_KEEP() backs up D500.., C100.., T100.. and M512.. into the retained
 * flash block, PLC_DATA_RECOVER() restores them when the marker and the CRC are valid. Both share
 * the staging buffer with the program download, so a staged program block is flushed first and
 * must not be lost when the flash fails.
 */
static void retention_tests(void) {
  reset();
  PLC_16BIT(Addr_D0000 + 1000) = 0x1234;  /* D500, the first retained data register */
  packet('E', "11808002ABCD"); ack(); PLC_DATA_KEEP();  /* KEEP flushes the staged program word first */
  assert(writes == 2 && bctrl1 == 255 && PLC_ROM_16BIT(0x80) == 0xCDAB);  /* both blocks written, nothing staged */
  /* RECOVER restores D500 (and flushes the word staged in the meantime). */
  PLC_16BIT(Addr_D0000 + 1000) = 0;
  packet('E', "1180820280FF"); ack(); PLC_DATA_RECOVER();
  assert(PLC_ROM_16BIT(0x82) == 0xFF80 && PLC_16BIT(Addr_D0000 + 1000) == 0x1234);
  /* A damaged retained block (wrong CRC) is ignored: the registers keep their values. */
  *(u16 *)PLC_RETAIN_BASE ^= 1U;
  PLC_16BIT(Addr_D0000 + 1000) = 0x5678; PLC_DATA_RECOVER();
  assert(PLC_16BIT(Addr_D0000 + 1000) == 0x5678);
  /* With a failing flash the staged program block stays staged and M8063 reports the error; the block
   * can still be written afterwards. */
  packet('E', "118084021122"); ack(); fail_flash = true;
  PLC_DATA_KEEP(); assert(bctrl1 == 0 && prog_write_buffer[0x84 / 2] == 0x2211);
  PLC_DATA_RECOVER(); assert(bctrl1 == 0 && prog_write_buffer[0x84 / 2] == 0x2211);
  assert(PLC_BIT_TEST(M8063)); fail_flash = false;
  packet('B', ""); ack(); assert(PLC_ROM_16BIT(0x84) == 0x2211);
}
/**
 * Monitor snapshot (E00 at address 0x1790). The host first stores a descriptor list in RAM: the
 * number of word entries at 0x1400, the number of bit entries at 0x1402 and one 16-bit entry per
 * item from 0x1404 (word entries are byte addresses, bit entries are bit addresses). Reading
 * 0x1790 then returns the current values, packed.
 */
static void monitor_tests(void) {
  reset();
  PLC_8BIT(0x1400) = 2; PLC_8BIT(0x1402) = 17;  /* 2 word entries, 17 bit entries */
  PLC_16BIT(0x1404) = 0x2000; PLC_16BIT(0x1406) = Addr_CHVal + 55 * 4;  /* words: D0000 and the 32-bit counter C255 */
  PLC_16BIT(0x2000) = 0x1234;
  PLC_16BIT(Addr_CHVal + 55 * 4) = 0x5678;
  PLC_16BIT(Addr_CHVal + 55 * 4 + 2) = 0x9ABC;
  for (unsigned i = 0; i < 17; i++) PLC_16BIT(0x1408 + i * 2U) = 0x80 + i;  /* bits: addresses 0x80..0x90 */
  PLC_BIT_ON(0x80); PLC_BIT_ON(0x90);  /* the first and the last bit are on */
  /* Reply: D0000 = 1234, C255 = 9ABC5678 (two words), then the 17 bits packed 16 per word. */
  packet('E', "0017900A"); response("34127856BC9A01000100");
  PLC_16BIT(0x1404) = 0xFFFF; packet('E', "0017900A"); nak();  /* word entry outside the image */
  /* A word entry with an odd byte address is refused. */
  PLC_16BIT(0x1408) = 0x80; PLC_16BIT(0x1404) = 0x2FFF;
  packet('E', "0017900A"); nak();
  /* Counts that do not fit the descriptor area are refused. */
  PLC_8BIT(0x1400) = 255; PLC_8BIT(0x1402) = 255;
  packet('E', "0017900A"); nak();
  /* 57 word entries: the packed values would not fit the monitor workspace (it ends at 0x1800). */
  PLC_8BIT(0x1400) = 57; PLC_8BIT(0x1402) = 0;
  for (unsigned i = 0; i < 57; i++) PLC_16BIT(0x1404 + i * 2U) = 0x2000;
  packet('E', "0017900A"); nak();
}
/**
 * Transmit handshaking and error interrupts. While a reply is pending or being sent, and while
 * the retained-data code owns the staging buffer, incoming frames are dropped and must neither
 * execute nor disturb the reply.
 */
static void dma_tests(void) {
  reset();
  staging_busy = 1; packet('1', "008001FF");  /* dropped: the staging buffer is owned by the retained-data code */
  assert(PLC_8BIT(0x80) == 0 && !UART_SEND_READY);
  staging_busy = 0;
  packet('0', "000002");  /* a read command leaves a pending reply */
  char saved[144]; memcpy(saved, tx_data, sizeof(saved));
  /* A frame that arrives while a reply is pending is dropped and leaves the reply intact. */
  packet('1', "008001FF"); assert(!memcmp(saved, tx_data, sizeof(saved)) && PLC_8BIT(0x80) == 0);
  TX_Process(); assert(!UART_Sended && (DMA1_Channel4->CCR & DMA_CCR_EN));  /* main loop starts the DMA */
  byte(5); packet('1', "008001FF");  /* dropped while the DMA transfer is running, even an ENQ */
  assert(!memcmp(saved, tx_data, sizeof(saved)) && PLC_8BIT(0x80) == 0);
  /* The transfer-complete interrupt releases the channel. */
  complete(); assert(UART_Sended && tx_count == 0 && !(DMA1_Channel4->CCR & DMA_CCR_EN));
  packet('1', "008001FF"); ack(); assert(PLC_8BIT(0x80) == 255);  /* accepted again */
  byte(5); assert(!UART_Sended && tx_count == 1);  /* ENQ is answered with ACK, transfer started */
  /* A DMA transfer error sets M8063. */
  DMA1->ISR = DMA_ISR_TEIF4; DMA1_Channel4_IRQHandler(); assert(PLC_BIT_TEST(M8063));
  /* A USART overrun error discards the frame being received. */
  byte(2); byte('0'); USART1->SR = USART_SR_RXNE | USART_SR_ORE;
  USART1->DR = '0'; USART1_IRQHandler(); assert(rx_count == 0);
  /* A normal received byte goes through the interrupt handler: the STX is stored at index 2. */
  USART1->SR = USART_SR_RXNE; USART1->DR = 2; USART1_IRQHandler(); assert(rx_count == 2);
  rx_count = 0;
  usart(9600); assert(USART1->BRR == 7500);  /* 72 MHz / 9600 baud */
}
/**
 * Systematic malformed input: truncated headers, invalid hexadecimal digits, wrong lengths and
 * writes to the protected M8000..M8007 byte. Every frame has a correct checksum, so the protocol
 * itself has to detect the problem and answer with NAK.
 */
static void malformed_matrix(void) {
  reset();
  /* Every fixed header truncation, after prior tx_data has been filled with
   * nonzero bytes. These frames all have VALID checksums. */
  const char *headers[] = { "00179002", "01008002", "10408002", "11808002", "41800080FF" };
  for (unsigned i = 0; i < sizeof(headers) / sizeof(headers[0]); i++) {
    unsigned min = i == 4 ? 10 : 8;
    for (unsigned n = 0; n < min; n++) {
      char shortened[11]; memcpy(shortened, headers[i], n); shortened[n] = 0;
      packet('E', shortened); nak();
    }
  }
  /* Invalid ASCII at each payload position, with checksum updated to match. */
  for (unsigned n = 0; n < 12; n++) {
    char invalid[] = "1180800280FF"; invalid[n] = 'Z';
    packet('E', invalid); nak();
  }
  /* A non-hexadecimal digit in each position of an E7 bit address. */
  packet('E', "7Z000"); nak(); packet('E', "70Z00"); nak();
  packet('E', "700Z0"); nak(); packet('E', "7000Z"); nak();
  /* Unknown command, unknown sub-command. */
  packet('9', "000001"); nak(); packet('E', "FF"); nak();
  packet('E', "10800002AABB"); nak(); /* unsupported parameter write is not ACKed */
  /* Read with an extra payload byte; write whose length byte does not match the data. */
  packet('0', "00000100"); nak(); packet('1', "000001AABB"); nak();
  /* E11 with length 0; E42 with an unexpected payload. */
  packet('E', "11008000"); nak(); packet('E', "4200"); nak();
  /* B with a payload; command 7 with a payload that is too long. */
  packet('B', "00"); nak(); packet('7', "008000"); nak();
  /* The byte that holds M8000..M8007 (0x01C0) is protected: a write that covers it is refused... */
  packet('1', "01C00109"); nak(); packet('1', "01BF02FF09"); nak();
  /* ...through E10 and E11 as well. */
  packet('E', "1001C00109"); nak(); packet('E', "1101C00109"); nak();
  /* Setting or resetting M8000..M8007 directly is refused... */
  packet('7', "0E00"); nak(); packet('8', "0E07"); nak();
  /* ...also with E7 and E8. */
  packet('E', "7000E"); nak(); packet('E', "8070E"); nak();
  /* None of these frames wrote to flash or started a program update. */
  assert(!writes && !PLC_UPDATE_PROG);
}
/**
 * A programming-tool session over the wire: connect, identify the PLC, stop it, download the
 * program LD M8000, OUT M0, END (words 01C2 8E00 C000 000F, sent low byte first), survive a
 * flash failure when RUN is requested, read the program back and start it.
 */
static void connect_upload_tests(void) {
  reset();
  usart(19200);
  /* Stray bytes outside a frame are ignored; 72 MHz / 19200 baud gives BRR = 3750. */
  byte(0xFF); byte('x'); assert(USART1->BRR == 3750 && !tx_count);
  byte(5); ack();  /* ENQ is answered with ACK */
  /* D8001 (model code 0x6266) and D8002 (program capacity in thousands of steps). */
  packet('0', "0E0204"); response("66620600");
  /* The relay byte M8000..M8007 reads 0x0A: the PLC is in STOP. */
  packet('0', "01C001"); response("0A");
  /* STOP request (M8037), then E61 discards any staged block. */
  packet('E', "7250E"); ack();
  packet('E', "61"); ack();
  /* E11 at 0x805C: the four program words, eight bytes with the low byte first. */
  packet('E', "11805C08C201008E00C00F00"); ack();
  /* RUN (M8036) flushes the staged block first; with a failing flash it is refused and the PLC
   * stays in STOP with the block still staged. */
  fail_flash = true;
  packet('E', "7240E"); nak(); assert(PLC_8BIT(Addr_M8) == 10 && bctrl1 == 0);
  fail_flash = false;
  /* After the flash recovers, E42 writes the block and the program reads back. */
  packet('E', "42"); ack();
  packet('E', "01805C08"); response("C201008E00C00F00");  /* read the program back */
  /* RUN now succeeds and the relay byte reads 0x09. */
  packet('E', "7240E"); ack();
  packet('0', "01C001"); response("09");
  packet('0', "000001"); response("00"); /* transport fixture does not execute ladder */
  /* STOP again. */
  packet('E', "7250E"); ack();
  packet('0', "01C001"); response("0A");
}
#ifdef TEST_WITH_RUNTIME
/**
 * Complete path with the real runtime: download over the wire, RUN request through the mailbox,
 * scan, clock relay M8013 and STOP. The interrupt only queues a request; the relays change when
 * main runs PLC_Main_Parser().
 */
static void integration_tests(void) {
  reset();
  byte(5); ack();
  packet('E', "11805C08C201008E00C00F00"); ack();  /* E11 at 0x805C: LD M8000, OUT M0, END */
  packet('E', "42"); ack();  /* write the block to flash */
  packet('E', "01805C08"); response("C201008E00C00F00");  /* read it back */
  packet('E', "7240E"); ack();  /* RUN request (M8036) */
  assert(!PLC_BIT_TEST(M8000) && PLC_Runtime_RunRequested()); /* IRQ only queues. */
  PLC_Main_Parser();
  /* Writes to the status byte (M8000..M8007) are refused. */
  packet('1', "01C0010A"); nak(); assert(PLC_BIT_TEST(M8000));
  packet('7', "0E01"); nak(); assert(!PLC_BIT_TEST(M8001));
  /* The program LD M8000, OUT M0 runs: M0 is on. */
  packet('0', "000001"); response("01");
  packet('E', "7250E"); ack(); PLC_Main_Parser();
  /* Second program: LD M8013, OUT Y0, END (words 01C2 8E0D CC00 000F). */
  packet('E', "11805C08C2010D8E00CC0F00"); ack();
  packet('E', "42"); ack();
  packet('E', "7240E"); ack(); PLC_Main_Parser();
  packet('0', "018001"); response("00");
  /* 500 simulated 1 ms ticks: M8013 (1 s clock) goes high, and so does Y0... */
  for (unsigned i = 0; i < 500; i++) PLC_Runtime_Tick1ms();
  PLC_Main_Parser();
  assert(PLC_BIT_TEST(M8013));
  packet('0', "018001"); response("01");
  /* ...500 more ticks: the clock is low again. */
  for (unsigned i = 0; i < 500; i++) PLC_Runtime_Tick1ms();
  PLC_Main_Parser();
  assert(!PLC_BIT_TEST(M8013));
  packet('0', "018001"); response("00");
  /* STOP clears the outputs. */
  packet('E', "7250E"); ack(); PLC_Main_Parser();
  assert(!PLC_BIT_TEST(M8000) && !PLC_8BIT(Addr_Y));  /* outputs are off */
  puts("PASS: actual protocol + runtime M0 and M8013->Y0 examples, simulated 1ms ticks, STOP clears outputs");
}
#endif
/**
 * Map the host memory that stands in for the device, then run either one of the special modes
 * (--integration, --wire) or all transport tests.
 */
int main(int argc, char **argv) {
  map_region(SRAM_BASE + 0x1000U, 0x1000U); /* Pulse workspace at 0x1C00. */
  map_region(RAM_ADDR, PLC_RAM_SIZE);
  map_region(0x08000000UL, 0x10000U);
  map_region(0x40000000UL, 0x24000U);
#ifdef TEST_WITH_RUNTIME
  if (argc == 2 && !strcmp(argv[1], "--integration")) {
    integration_tests(); return 0;
  }
#endif
  /* Pipe mode: every byte read from stdin is a received USART byte; a reply is written to stdout
   * as soon as it is ready. The runtime build also ticks the clock from host time and scans. */
  if (argc == 2 && !strcmp(argv[1], "--wire")) {
    reset();
#ifdef TEST_WITH_RUNTIME
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    uint64_t elapsed_ms = 0;
#endif
    int value;
    while ((value = getchar()) != EOF) {
      byte((u8)value);
#ifdef TEST_WITH_RUNTIME
      /* Simulated TIM2 ticks track host wall time for the complete CLI smoke.
       * The physical timer ISR is not emulated by this pipe fixture. */
      clock_gettime(CLOCK_MONOTONIC, &now);
      uint64_t current_ms = ((uint64_t)(now.tv_sec - start.tv_sec) * 1000000000ULL +
                             now.tv_nsec - start.tv_nsec) / 1000000ULL;
      while (elapsed_ms < current_ms) { PLC_Runtime_Tick1ms(); elapsed_ms++; }
      PLC_Main_Parser();
#endif
      if (UART_SEND_READY) TX_Process();
      if (!UART_Sended) {
        assert(fwrite(tx_data, 1, tx_count, stdout) == tx_count);
        fflush(stdout);
        complete();
      }
    }
    return 0;
  }
  /* The transport tests; each one starts with reset(), so they do not depend on each other. */
  force_tests(); bounds_tests(); flash_tests(); search_tests();
  retention_tests(); monitor_tests(); dma_tests(); malformed_matrix(); connect_upload_tests();
  printf("PASS: %u framed protocol cases plus DMA, overflow, retention and malformed-stream checks (ASan/UBSan)\n", cases);
  return 0;
}
