#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include "../../../src/PLC_Com.c"

#ifdef TEST_WITH_RUNTIME
#include "../../../src/ladder.c"
#if PLC_RUNTIME_EXPERIMENTAL
volatile u16 PLC_RUN_TIME;
u16 X_DIY;
void RTC_Set(u16 year,u8 month,u8 day,u8 hour,u8 minute,u8 second) {
  (void)year;(void)month;(void)day;(void)hour;(void)minute;(void)second;
}
#endif
#else
volatile u8 PLC_NEW_PROG;
static int requested_run = -1;
void PLC_Runtime_RequestRun(u8 run) { requested_run = run != 0; }
#endif
static unsigned writes, erased_pages, cases;
static bool fail_flash;
bool plc_ram_range(uint32_t offset, uint32_t length) {
  return offset <= PLC_RAM_SIZE && length <= PLC_RAM_SIZE - offset;
}
u32 board_apb2_hz(void) { return 72000000U; }
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
bool plc_flash_write_block(uint32_t address, const uint16_t *data) {
  assert(address >= ROM_ADDR && address <= PLC_FLASH_END - PLC_FLASH_BLOCK_SIZE);
  assert(!(address & (PLC_FLASH_BLOCK_SIZE - 1U)));
  if (fail_flash) return false;
  writes++; erased_pages += 2;
  memcpy((void *)(uintptr_t)address, data, PLC_FLASH_BLOCK_SIZE);
  return true;
}
bool plc_flash_erase_block(uint32_t address) {
  assert(address >= ROM_ADDR && address < PLC_RETAIN_BASE);
  assert(!(address & (PLC_FLASH_BLOCK_SIZE - 1U)));
  if (fail_flash) return false;
  erased_pages += 2;
  memset((void *)(uintptr_t)address, 255, PLC_FLASH_BLOCK_SIZE);
  return true;
}
static void map_region(uintptr_t address, size_t length) {
  assert(mmap((void *)address, length, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == (void *)address);
}
static void reset(void) {
  memset((void *)RAM_ADDR, 0, PLC_RAM_SIZE);
  memset((void *)0x08000000UL, 255, 0x10000U);
  memcpy((void *)ROM_ADDR, PLC_ROM, sizeof(PLC_ROM)); /* Factory .plc_rom image. */
  memset((void *)0x40000000UL, 0, 0x24000U);
  memset(tx_data, 0xAB, sizeof(tx_data));
  memset(rx_data, 0, sizeof(rx_data));
  rx_count = tx_count = 0; UART_Sended = 1; UART_SEND_READY = 0;
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
static void complete(void) {
  DMA1->ISR = DMA_ISR_TCIF4; DMA1_Channel4_IRQHandler();
  DMA1->ISR = 0;
}
static void release(void) {
  if (UART_SEND_READY) TX_Process();
  if (!UART_Sended) complete();
}
/* Wire framing is the original STX, command, ASCII payload, ETX, checksum.
 * RX stores the STX at array index 2, so last index = wire length + 1. */
static void packet(char command, const char *hex) {
  unsigned sum = (u8)command + 3U;
  byte(2); byte(command);
  for (size_t i = 0; hex[i]; i++) { sum += (u8)hex[i]; byte(hex[i]); }
  byte(3); byte(ASCII[(sum & 255U) >> 4]); byte(ASCII[sum & 15U]);
  cases++;
}
static void ack(void) { assert((u8)tx_data[0] == 6 && tx_count == 1); release(); }
static void nak(void) {
  if ((u8)tx_data[0] != 21 || tx_count != 1)
    fprintf(stderr, "NAK expected in case %u, got byte %02X count %u\n", cases, (u8)tx_data[0], tx_count);
  assert((u8)tx_data[0] == 21 && tx_count == 1); release();
}
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
static void force_tests(void) {
  reset();
  assert(PLC_8BIT(Addr_M8) == 10); /* Boot STOP. */
  packet('E', "78000"); ack(); assert(PLC_BIT_TEST(0x80) && PLC_8BIT(Addr_M8) == 10);
  packet('E', "88000"); ack(); assert(!PLC_BIT_TEST(0x80));
  packet('E', "7250E"); ack(); assert(PLC_8BIT(Addr_M8) == 10);
  PLC_UPDATE_PROG = 1;
  packet('E', "7240E"); ack(); assert(PLC_8BIT(Addr_M8) == 9 && !PLC_UPDATE_PROG);
  packet('E', "7230E"); ack();
  packet('E', "7G000"); nak();
  packet('E', "7FFFF"); ack(); assert(PLC_BIT_TEST(0xFFFF));
  packet('7', "0080"); ack(); assert(PLC_BIT_TEST(0x80));
  packet('8', "0080"); ack(); assert(!PLC_BIT_TEST(0x80));
}
static void bounds_tests(void) {
  reset();
  packet('1', "00800280FF"); ack();
  assert(PLC_8BIT(0x80) == 0x80 && PLC_8BIT(0x81) == 255);
  packet('0', "008002"); response("80FF");
  packet('E', "10408002ABCD"); ack();
  packet('E', "00408002"); response("ABCD");
  packet('1', "2FFF01FE"); ack();
  packet('0', "2FFF01"); response("FE");
  packet('0', "2FFF02"); nak();
  packet('1', "2FFF02AABB"); nak();
  packet('0', "000046"); assert(tx_count == 144); release();
  packet('0', "000047"); nak();
  char maximum[137];
  strcpy(maximum, "010041");
  memset(maximum + 6, 'A', 130); maximum[136] = 0;
  packet('1', maximum); ack();
  for (unsigned i = 0; i < 65; i++) assert(PLC_8BIT(0x100 + i) == 0xAA);
  packet('0', "800001"); nak(); /* high byte must not sign-extend */
  packet('7', "6000"); ack(); assert(PLC_BIT_TEST(0x6000));
  packet('E', "10408002AA"); nak(); /* missing final data byte */
  packet('1', "008002AA"); nak();
  packet('1', "008001aF"); ack(); assert(PLC_8BIT(0x80) == 0xAF);
  const char commands[] = "0178E";
  for (unsigned i = 0; i < sizeof(commands) - 1U; i++) {
    packet(commands[i], ""); nak();
    packet(commands[i], "00"); nak();
  }
  packet('E', "00"); nak(); packet('E', "41"); nak();
  packet('E', "11008001"); nak();
  packet('E', "ZZ"); nak(); packet('0', "00000"); nak();
  /* Short, checksum-correct commands must not reuse the last response. */
  PLC_8BIT(0x17F) = 0xFF;
  packet('E', "62"); ack(); assert(PLC_8BIT(0x17F) == 0);
  packet('E', "63"); ack(); packet('E', "64"); ack();
  packet('4', ""); ack(); packet('B', ""); ack();
  byte(2); byte('0'); byte('0'); byte(3); byte('G'); byte('G'); nak();
  byte(2); byte('0'); byte(3); byte('0'); byte('0'); nak();
  byte(2); byte('0'); byte('0'); byte('0'); byte('0'); byte('0'); byte('0'); byte('1');
  byte(3); byte('0'); byte('0'); nak(); /* valid hex, incorrect checksum */
  byte(2); byte('1'); byte('0'); assert(rx_count > 0 && !UART_SEND_READY);
  packet('0', "008001"); response("00"); /* new STX resynchronizes */
  byte(2); for (unsigned i = 0; i < 142; i++) byte('A'); nak();
}
static void flash_tests(void) {
  reset();
  memset((void *)ROM_ADDR, 0x5A, PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE);
  packet('E', "1180800280FF"); ack();
  assert(!writes && prog_write_buffer[0x80 / 2] == 0xFF80);
  packet('E', "1187FE0411223344"); ack(); /* crosses 2 KB boundary */
  assert(writes == 1 && PLC_ROM_16BIT(0x80) == 0xFF80);
  assert(PLC_ROM_16BIT(0x7FE) == 0x2211 && PLC_ROM_16BIT(0x800) == 0x5A5A);
  packet('E', "42"); ack();
  assert(writes == 2 && PLC_ROM_16BIT(0x800) == 0x4433);
  assert(PLC_ROM_16BIT(0x82) == 0x5A5A && PLC_ROM_16BIT(0x802) == 0x5A5A);
  packet('E', "11808202ABCD"); ack(); packet('B', ""); ack();
  assert(PLC_ROM_16BIT(0x82) == 0xCDAB);
  packet('E', "118081021122"); nak(); /* unaligned */
  packet('E', "1180800111"); nak(); /* odd length */
  packet('E', "11B7FE0411223344"); nak();
  packet('E', "01B7FF01"); response("5A");
  packet('E', "0137FF01"); response("5A");
  packet('E', "01B7FF02"); nak();
  packet('E', "01B80001"); nak();
  packet('E', "118100023412"); ack();
  fail_flash = true; packet('E', "42"); nak(); assert(bctrl1 == 0);
  fail_flash = false; packet('E', "42"); ack(); assert(PLC_ROM_16BIT(0x100) == 0x1234);
  packet('E', "118102024321"); ack(); packet('E', "61"); ack();
  packet('B', ""); ack(); assert(PLC_ROM_16BIT(0x102) == 0x5A5A);
  unsigned pages = erased_pages;
  packet('E', "60"); ack();
  assert(erased_pages - pages == PLC_PROGRAM_BLOCKS * 2U);
  assert(PLC_ROM_16BIT(0x5A) == 0x5A5A && PLC_ROM_16BIT(0x5C) == 15);
  for (u32 i = 0x5E; i < PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE; i++)
    assert(PLC_ROM_8BIT(i) == 255);
  assert(*(u16 *)PLC_RETAIN_BASE == 0xFFFF);
  packet('E', "11008002AABB"); ack();
  assert(PLC_8BIT(0x80) == 0xAA && PLC_UPDATE_PROG && PLC_8BIT(Addr_M8) == 10);
}
static void search_tests(void) {
  reset();
  PLC_ROM_16BIT(0x100) = 0xFF80;
  packet('E', "41800080FF");
  assert(tx_count == 9 && !memcmp(tx_data + 1, "18100", 5));
  unsigned sum = 3; for (unsigned i = 1; i <= 5; i++) sum += tx_data[i];
  assert(tx_data[7] == ASCII[(sum & 255) >> 4] && tx_data[8] == ASCII[sum & 15]); release();
  packet('E', "41B7FE80FF"); assert(!memcmp(tx_data + 2, "B800", 4)); release();
  packet('E', "417FFF80FF"); assert(!memcmp(tx_data + 2, "FFFF", 4)); release();
}
static void retention_tests(void) {
  reset();
  PLC_16BIT(Addr_D0000 + 1000) = 0x1234;
  packet('E', "11808002ABCD"); ack(); PLC_DATA_KEEP();
  assert(writes == 2 && bctrl1 == 255 && PLC_ROM_16BIT(0x80) == 0xCDAB);
  PLC_16BIT(Addr_D0000 + 1000) = 0;
  packet('E', "1180820280FF"); ack(); PLC_DATA_RECOVER();
  assert(PLC_ROM_16BIT(0x82) == 0xFF80 && PLC_16BIT(Addr_D0000 + 1000) == 0x1234);
  *(u16 *)PLC_RETAIN_BASE ^= 1U;
  PLC_16BIT(Addr_D0000 + 1000) = 0x5678; PLC_DATA_RECOVER();
  assert(PLC_16BIT(Addr_D0000 + 1000) == 0x5678);
  packet('E', "118084021122"); ack(); fail_flash = true;
  PLC_DATA_KEEP(); assert(bctrl1 == 0 && prog_write_buffer[0x84 / 2] == 0x2211);
  PLC_DATA_RECOVER(); assert(bctrl1 == 0 && prog_write_buffer[0x84 / 2] == 0x2211);
  assert(PLC_BIT_TEST(M8063)); fail_flash = false;
  packet('B', ""); ack(); assert(PLC_ROM_16BIT(0x84) == 0x2211);
}
static void monitor_tests(void) {
  reset();
  PLC_8BIT(0x1400) = 2; PLC_8BIT(0x1402) = 17;
  PLC_16BIT(0x1404) = 0x2000; PLC_16BIT(0x1406) = Addr_CHVal + 55 * 4;
  PLC_16BIT(0x2000) = 0x1234;
  PLC_16BIT(Addr_CHVal + 55 * 4) = 0x5678;
  PLC_16BIT(Addr_CHVal + 55 * 4 + 2) = 0x9ABC;
  for (unsigned i = 0; i < 17; i++) PLC_16BIT(0x1408 + i * 2U) = 0x80 + i;
  PLC_BIT_ON(0x80); PLC_BIT_ON(0x90);
  packet('E', "0017900A"); response("34127856BC9A01000100");
  PLC_16BIT(0x1404) = 0xFFFF; packet('E', "0017900A"); nak();
  PLC_16BIT(0x1408) = 0x80; PLC_16BIT(0x1404) = 0x2FFF;
  packet('E', "0017900A"); nak();
  PLC_8BIT(0x1400) = 255; PLC_8BIT(0x1402) = 255;
  packet('E', "0017900A"); nak();
  PLC_8BIT(0x1400) = 57; PLC_8BIT(0x1402) = 0;
  for (unsigned i = 0; i < 57; i++) PLC_16BIT(0x1404 + i * 2U) = 0x2000;
  packet('E', "0017900A"); nak();
}
static void dma_tests(void) {
  reset();
  staging_busy = 1; packet('1', "008001FF");
  assert(PLC_8BIT(0x80) == 0 && !UART_SEND_READY);
  staging_busy = 0;
  packet('0', "000002");
  char saved[144]; memcpy(saved, tx_data, sizeof(saved));
  packet('1', "008001FF"); assert(!memcmp(saved, tx_data, sizeof(saved)) && PLC_8BIT(0x80) == 0);
  TX_Process(); assert(!UART_Sended && (DMA1_Channel4->CCR & DMA_CCR_EN));
  byte(5); packet('1', "008001FF");
  assert(!memcmp(saved, tx_data, sizeof(saved)) && PLC_8BIT(0x80) == 0);
  complete(); assert(UART_Sended && tx_count == 0 && !(DMA1_Channel4->CCR & DMA_CCR_EN));
  packet('1', "008001FF"); ack(); assert(PLC_8BIT(0x80) == 255);
  byte(5); assert(!UART_Sended && tx_count == 1);
  DMA1->ISR = DMA_ISR_TEIF4; DMA1_Channel4_IRQHandler(); assert(PLC_BIT_TEST(M8063));
  byte(2); byte('0'); USART1->SR = USART_SR_RXNE | USART_SR_ORE;
  USART1->DR = '0'; USART1_IRQHandler(); assert(rx_count == 0);
  USART1->SR = USART_SR_RXNE; USART1->DR = 2; USART1_IRQHandler(); assert(rx_count == 2);
  rx_count = 0;
  usart(9600); assert(USART1->BRR == 7500);
}
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
  packet('E', "7Z000"); nak(); packet('E', "70Z00"); nak();
  packet('E', "700Z0"); nak(); packet('E', "7000Z"); nak();
  packet('9', "000001"); nak(); packet('E', "FF"); nak();
  packet('E', "10800002AABB"); nak(); /* unsupported parameter write is not ACKed */
  packet('0', "00000100"); nak(); packet('1', "000001AABB"); nak();
  packet('E', "11008000"); nak(); packet('E', "4200"); nak();
  packet('B', "00"); nak(); packet('7', "008000"); nak();
  packet('1', "01C00109"); nak(); packet('1', "01BF02FF09"); nak();
  packet('E', "1001C00109"); nak(); packet('E', "1101C00109"); nak();
  packet('7', "0E00"); nak(); packet('8', "0E07"); nak();
  packet('E', "7000E"); nak(); packet('E', "8070E"); nak();
  assert(!writes && !PLC_UPDATE_PROG);
}
static void connect_upload_tests(void) {
  reset();
  usart(19200);
  byte(0xFF); byte('x'); assert(USART1->BRR == 3750 && !tx_count);
  byte(5); ack();
  packet('0', "0E0204"); response("66620600");
  packet('0', "01C001"); response("0A");
  packet('E', "7250E"); ack();
  packet('E', "61"); ack();
  packet('E', "11805C08C201008E00C00F00"); ack();
  fail_flash = true;
  packet('E', "7240E"); nak(); assert(PLC_8BIT(Addr_M8) == 10 && bctrl1 == 0);
  fail_flash = false;
  packet('E', "42"); ack();
  packet('E', "01805C08"); response("C201008E00C00F00");
  packet('E', "7240E"); ack();
  packet('0', "01C001"); response("09");
  packet('0', "000001"); response("00"); /* transport fixture does not execute ladder */
  packet('E', "7250E"); ack();
  packet('0', "01C001"); response("0A");
}
#ifdef TEST_WITH_RUNTIME
static void integration_tests(void) {
  reset();
  byte(5); ack();
  packet('E', "11805C08C201008E00C00F00"); ack();
  packet('E', "42"); ack();
  packet('E', "01805C08"); response("C201008E00C00F00");
  packet('E', "7240E"); ack();
  assert(!PLC_BIT_TEST(M8000) && PLC_Runtime_RunRequested()); /* IRQ only queues. */
  PLC_Main_Parser();
  packet('1', "01C0010A"); nak(); assert(PLC_BIT_TEST(M8000));
  packet('7', "0E01"); nak(); assert(!PLC_BIT_TEST(M8001));
  packet('0', "000001"); response("01");
  packet('E', "7250E"); ack(); PLC_Main_Parser();
  packet('E', "11805C08C2010D8E00CC0F00"); ack();
  packet('E', "42"); ack();
  packet('E', "7240E"); ack(); PLC_Main_Parser();
  packet('0', "018001"); response("00");
  for (unsigned i = 0; i < 500; i++) PLC_Runtime_Tick1ms();
  PLC_Main_Parser();
  assert(PLC_BIT_TEST(M8013));
  packet('0', "018001"); response("01");
  for (unsigned i = 0; i < 500; i++) PLC_Runtime_Tick1ms();
  PLC_Main_Parser();
  assert(!PLC_BIT_TEST(M8013));
  packet('0', "018001"); response("00");
  packet('E', "7250E"); ack(); PLC_Main_Parser();
  assert(!PLC_BIT_TEST(M8000) && !PLC_8BIT(Addr_Y));
  puts("PASS: actual protocol + runtime M0 and M8013->Y0 examples, simulated 1ms ticks, STOP clears outputs");
}
#endif
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
  force_tests(); bounds_tests(); flash_tests(); search_tests();
  retention_tests(); monitor_tests(); dma_tests(); malformed_matrix(); connect_upload_tests();
  printf("PASS: %u framed protocol cases plus DMA, overflow, retention and malformed-stream checks (ASan/UBSan)\n", cases);
  return 0;
}
