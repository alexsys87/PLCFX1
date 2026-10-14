/**
 * @file    plc_serial_protocol.c
 * @brief   Programming-port protocol of the PLC: USART1/DMA link, memory and
 *          program access, retained-data storage and the factory register image.
 *
 * GX-style tools (and tools/plc_link.py) use this link to read and write PLC
 * memory, download and read back the program, force bits, clear memory areas
 * and switch the PLC between RUN and STOP.
 *
 * Origin: derived from the register-level implementation by Xiao Xiaoying
 * (edit date 2015-09-09, shop182385147.taobao.com), which was tested on the
 * STM32F103RDT6 and STM32F103VET6. This version uses CMSIS register access
 * only and targets the STM32F103C8 "BluePill" (20 KB RAM, 64 KB flash).
 *
 * Link layer
 * ----------
 * The port runs at 19200 baud, 8N1 (configured in App/main.c).
 *
 *     request   ENQ                          -> reply ACK
 *     request   STX cmd payload ETX cs cs    -> reply ACK, NAK or a data frame
 *     reply     STX hh hh ... hh ETX cs cs   (data frame)
 *
 *   STX = 0x02, ETX = 0x03, ENQ = 0x05, ACK = 0x06, NAK = 0x15.
 *   cmd      one ASCII character that selects the command.
 *   payload  hexadecimal digit pairs (either case), most significant digit first.
 *   hh       one data byte as two upper-case hexadecimal digits.
 *   cs cs    two upper-case hexadecimal digits holding the low 8 bits of the sum
 *            of all bytes between STX and the checksum digits (cmd up to and
 *            including ETX).
 *
 * A reply carries at most 70 data bytes (144-byte buffer). A new STX restarts
 * frame reception. Any malformed frame, bad checksum, out-of-range address,
 * protected target or flash failure is answered with NAK.
 *
 * Commands
 * --------
 *   cmd  payload (bytes)             action
 *   '0'  addr(2) len(1)              read len bytes of PLC RAM at offset addr
 *   '1'  addr(2) len(1) data(len)    write PLC RAM
 *   '4'  any even number of bytes    legacy handshake, always answered with ACK
 *   '7'  addr(2)                     set the bit with bit address addr
 *   '8'  addr(2)                     reset the bit with bit address addr
 *   'B'  none                        write the staged program block to flash
 *   'E'  sub(1) ...                  extended commands, see below
 *
 * All 16-bit addresses are transmitted high byte first. Writes that touch the
 * byte holding M8000..M8007 are refused (see writable_ram()).
 *
 * Extended commands ('E' followed by the sub-command byte)
 * --------------------------------------------------------
 *   E00  addr(2) len(1)              read RAM; addr >= 0x4000 is shifted down by
 *                                    0x2000 (host address of D0000 is 0x4000);
 *                                    addr 0x1790 first rebuilds the monitor
 *                                    snapshot (see build_monitor())
 *   E01  addr(2) len(1)              read program memory (flag 0x8000 removed)
 *   E10  addr(2) len(1) data(len)    write RAM (same D shift as E00)
 *   E11  addr(2) len(1) data(len)    write program memory: addr >= 0x8000 stages
 *                                    little-endian words in the flash block
 *                                    buffer, addr < 0x8000 writes RAM bytes;
 *                                    either form stops the PLC and flags a
 *                                    program update
 *   E41  addr(2) lo(1) hi(1)         search the program from addr for the word
 *                                    hi:lo; the reply is '1' plus the address
 *   E42  none                        write the staged program block to flash
 *   E60  none                        clear program memory (END instruction only)
 *   E61  none                        discard the staged program block
 *   E62  none                        clear M0..M3071
 *   E63  none                        clear D0..D1999 (MAXDXXX words)
 *   E64  none                        clear D0..D999
 *
 * Bit forcing with E7/E8: the frame  STX 'E' '7'|'8' d0 d1 d2 d3 ETX cs cs
 * carries the bit address as four hex digits with the LOW byte first (d0 d1 is
 * the low byte, d2 d3 the high byte). E7 sets the bit and E8 resets it. M8035
 * (0x0E23) is ignored on E7; M8036 (0x0E24) requests RUN after the staged
 * program block was written to flash, and M8037 (0x0E25) requests STOP; both go
 * through PLC_Runtime_RequestRun() instead of changing the relay directly.
 *
 * Execution model
 * ---------------
 * RX_Process() runs in the USART1 interrupt and consumes one byte per call. A
 * complete frame with a valid checksum is executed at once by Process_switch()
 * in that interrupt; the reply is left in tx_data[] and UART_SEND_READY is set.
 * The main loop then calls TX_Process(), which starts the DMA transfer, and the
 * DMA transfer-complete interrupt releases the channel. While a reply is pending
 * or being sent, or while the retained-data code owns the flash staging buffer,
 * incoming bytes are consumed and dropped; the host repeats the request after
 * its time-out. Programming flash disables interrupts for the duration of one
 * block write.
 *
 * Program and retained data in flash
 * ----------------------------------
 * The program area starts at PLC_PROGRAM_BASE (ROM_ADDR) and has
 * PLC_PROGRAM_BLOCKS blocks of PLC_FLASH_BLOCK_SIZE bytes. Program words begin
 * at offset START_ADDR (0x5C); the bytes before them are the parameter header
 * (see PLC_ROM). Downloaded words are collected block by block in
 * prog_write_buffer (read-modify-write) and written when the download moves to
 * another block or when the host flushes ('B', E42 or RUN).
 *
 * The retained block (PLC_RETAIN_BASE, "block 10" in the legacy numbering) is a
 * backup of the latched devices, written by PLC_DATA_KEEP() and checked by
 * PLC_DATA_RECOVER():
 *
 *     words    0..449    D500..D949
 *     words  450..499    C100..C149 (16-bit counter values)
 *     words  500..549    T100..T149 (timer values)
 *     words  550..581    M512..M1023
 *     words  582..1021   0xFFFF (unused)
 *     word        1022   CRC-16 (reflected polynomial 0xA001, initial value
 *                        0xFFFF) over words 0..1021
 *     word        1023   validity marker 0xA55A, programmed last
 *
 * The firmware itself currently calls only PLC_DATA_INIT(); the backup and
 * restore functions are ready for a power-fail hook and are covered by the host
 * tests (tools/test_protocol.py).
 *
 * Special-register reference
 * --------------------------
 * The notes below come from the original source. They describe the FX special
 * devices that the factory image special_d[] provides; only the values listed
 * in that table are initialised, and the communication format D8120 is not
 * interpreted by this port (the link is fixed at 19200 baud, 8N1).
 *
 *   M8126 global flag                 M8127 request handshake signal
 *   M8128 request error flag          M8129 request switching / time-out
 *   D8013..D8019 clock: second, minute, hour, day, month, year, weekday
 *
 *   D8120 communication format (bit 0 is the least significant bit)
 *   +-----------+--------------+-----------------------------------------------+
 *   | bit       | meaning      | value                                         |
 *   +-----------+--------------+-----------------------------------------------+
 *   | b0        | data length  | 0: 7 bits, 1: 8 bits                          |
 *   | b2 b1     | parity       | 00: none, 01: odd, 11: even                   |
 *   | b3        | stop bits    | 0: 1 bit, 1: 2 bits                           |
 *   | b7..b4    | baud rate    | 0100: 600, 0101: 1200, 0110: 2400,            |
 *   |           |              | 0111: 4800, 1000: 9600, 1001: 19200           |
 *   | b8        | header       | 0: none, 1: D8124 (default STX)               |
 *   | b9        | terminator   | 0: none, 1: D8125 (default ETX)               |
 *   | b12..b10  | interface    | 000: RS-485/RS-422, 010: RS-232               |
 *   | b13       | sum check    | 0: none, 1: automatic sum code                |
 *   | b14       | protocol     | 0: no protocol, 1: dedicated protocol         |
 *   | b15       | format       | 0: format 1, 1: format 4                      |
 *   +-----------+--------------+-----------------------------------------------+
 *   Example: D8120 = 0x4096 selects 19200 baud.
 */

#include "platform.h"
#include "plc_flash.h"
#include "plc_memory.h"
#include "plc_runtime.h"
#include "board_clock.h"

#include <stdio.h>
#include "plc_memory_map.h"
#include "plc_io.h"
#include "plc_special_devices.h"
#include "plc_serial_protocol.h"

/* Linear bit access of the active runtime (also declared in plc_runtime.h). */
extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

/**
 * Factory image of the first bytes of the program flash area (ROM_ADDR).
 *
 * The linker places this section at the start of the program area, so a freshly
 * flashed device already holds an empty program: the parameter header below
 * followed by a single END instruction (0x000F) at START_ADDR (offset 0x5C).
 * All words are little-endian. The array is larger than the initializer; the
 * trailing bytes are zero.
 */
#ifdef __ICCARM__
#pragma location=".plc_rom"
__root const u8 PLC_ROM[110] = {
#else
const u8 PLC_ROM[110] __attribute__((section(".plc_rom"), used)) = {
#endif
  /* Offset 0x00: capacity in thousands of 16-bit program words (the configured
   * MAXSTEP, not a claim of compatibility with a commercial PLC). */
  MAXSTEP / 1000, 0x00,
  0x00, 0x00,                     /* 0x02 reserved */
  0x00, 0x00,                     /* 0x04 reserved */
  0x00, 0x00,                     /* 0x06 reserved */

  0x20, 0x20,                     /* 0x08 password, characters 0-1 (blank) */
  0x20, 0x20,                     /* 0x0A password, characters 2-3 */
  0x20, 0x20,                     /* 0x0C password, characters 4-5 */
  0x20, 0x20,                     /* 0x0E password, characters 6-7 */

  0x20, 0x20,                     /* 0x10 PLC name, characters 0-1 (blank) */
  0x20, 0x20,                     /* 0x12 PLC name, characters 2-3 */
  0x20, 0x20,                     /* 0x14 PLC name, characters 4-5 */
  0x20, 0x20,                     /* 0x16 PLC name, characters 6-7 */
  0x20, 0x20,                     /* 0x18 PLC name, characters 8-9 */
  0x20, 0x20,                     /* 0x1A PLC name, characters 10-11 */
  0x20, 0x20,                     /* 0x1C PLC name, characters 12-13 */
  0x20, 0x20,                     /* 0x1E PLC name, characters 14-15 */
  0x20, 0x20,                     /* 0x20 PLC name, characters 16-17 */
  0x20, 0x20,                     /* 0x22 PLC name, characters 18-19 */
  0x20, 0x20,                     /* 0x24 PLC name, characters 20-21 */
  0x20, 0x20,                     /* 0x26 PLC name, characters 22-23 */
  0x20, 0x20,                     /* 0x28 PLC name, characters 24-25 */
  0x20, 0x20,                     /* 0x2A PLC name, characters 26-27 */
  0x20, 0x20,                     /* 0x2C PLC name, characters 28-29 */
  0x20, 0x20,                     /* 0x2E PLC name, characters 30-31 */

  0x00, 0x00,                     /* 0x30 M latch range, start */
  0x00, 0x00,                     /* 0x32 M latch range, end */
  0x00, 0x00,                     /* 0x34 S latch range, start */
  0x00, 0x00,                     /* 0x36 S latch range, end */
  0x00, 0x0E,                     /* 0x38 C (16-bit) latch range, start */
  0x00, 0x0E,                     /* 0x3A C (16-bit) latch range, end */
  0xC8, 0x0E,                     /* 0x3C C (32-bit) latch range, start */
  0xFF, 0x0E,                     /* 0x3E C (32-bit) latch range, end */

  0x00, 0x00,                     /* 0x40 D latch range, start */
  0x00, 0x00,                     /* 0x42 D latch range, end */
  0x00, 0x00,                     /* 0x44 reserved */
  0x00, 0x00,                     /* 0x46 reserved */
  0x00, 0x00,                     /* 0x48 file register address */
  0x00, 0x00,                     /* 0x4A file register capacity (blocks) */
  0x00, 0x00,                     /* 0x4C comment area address */
  0x00, 0x00,                     /* 0x4E comment area capacity (blocks) */

  0x00, 0x00,                     /* 0x50 system flags (original note: 0x80 RUN input X007..X000, 0x40 PLC system) */
  0x00, 0x00,                     /* 0x52 reserved */
  0x00, 0x00,                     /* 0x54 reserved */
  0x00, 0x00,                     /* 0x56 reserved */
  0x00, 0x00,                     /* 0x58 reserved */
  0x00, 0x00,                     /* 0x5A reserved */

  0x0F, 0x00,                     /* 0x5C program start: END instruction */
  0xFF, 0xFF,                     /* 0x5E erased */
  0xFF, 0xFF,                     /* 0x60 erased */
  0xFF, 0xFF,                     /* 0x62 erased */
  0xFF, 0xFF};                    /* 0x64 erased */

/**
 * Factory values of the special data registers D8000..D8129. PLC_DATA_INIT()
 * copies them into the PLC register image at every boot. Entries without a
 * description were not documented in the original source and keep their
 * inherited value.
 */
const u16 special_d[130] = {
  0x00C8,               /* D8000: watchdog timer preset, 200 ms */
  0x6266,               /* D8001: PLC model/version code inherited from the original source; GX compatibility is unverified */
  MAXSTEP / 1000,       /* D8002: program capacity in thousands of steps (MAXSTEP / 1000) */
  0x0000,               /* D8003: memory type: 0x10 CPU RAM, 0x00 RAM, 0x01 EPROM, 0x02 EEPROM */
  0x0000,               /* D8004: error code */
  0x0025,               /* D8005: battery voltage 3.7 V (unit 0.1 V) */
  0x001E,               /* D8006: low-battery threshold 3.0 V (unit 0.1 V) */
  0x0000,               /* D8007 */
  0xFFFF,               /* D8008 */
  0x0000,               /* D8009 */

  0x0000,               /* D8010 */
  0x0000,               /* D8011 */
  0x0000,               /* D8012 */
  0x0000,               /* D8013: clock: second */
  0x0000,               /* D8014: clock: minute */
  0x0000,               /* D8015: clock: hour */
  0x0000,               /* D8016: clock: day of month */
  0x0000,               /* D8017: clock: month */
  0x0000,               /* D8018: clock: year */
  0x0000,               /* D8019: clock: day of week */

  0x000A,               /* D8020 */
  0x0000,               /* D8021 */
  0x0000,               /* D8022 */
  0x0000,               /* D8023 */
  0x0000,               /* D8024 */
  0x0000,               /* D8025 */
  0x0000,               /* D8026 */
  0x0000,               /* D8027 */
  0x0000,               /* D8028 */
  0x0000,               /* D8029 */

  0x0000,               /* D8030: analog input AN1 */
  0x0000,               /* D8031: analog input AN2 */
  0x0000,               /* D8032 */
  0x0000,               /* D8033 */
  0x0000,               /* D8034 */
  0x0000,               /* D8035 */
  0x0000,               /* D8036 */
  0x0000,               /* D8037 */
  0x0000,               /* D8038 */
  0x0000,               /* D8039 */

  0xFFFF,               /* D8040 */
  0xFFFF,               /* D8041 */
  0xFFFF,               /* D8042 */
  0xFFFF,               /* D8043 */
  0xFFFF,               /* D8044 */
  0xFFFF,               /* D8045 */
  0xFFFF,               /* D8046 */
  0xFFFF,               /* D8047 */
  0x0000,               /* D8048 */
  0xFFFF,               /* D8049 */

  0x0000,               /* D8050 */
  0x0000,               /* D8051 */
  0x0000,               /* D8052 */
  0x0000,               /* D8053 */
  0x0000,               /* D8054 */
  0x0000,               /* D8055 */
  0x0000,               /* D8056 */
  0x0000,               /* D8057 */
  0x0000,               /* D8058 */
  0x0000,               /* D8059 */

  0x0000,               /* D8060 */
  0x0000,               /* D8061 */
  0x183B,               /* D8062 */
  0x0000,               /* D8063 */
  0x0000,               /* D8064 */
  0x0000,               /* D8065 */
  0x0000,               /* D8066 */
  0x0000,               /* D8067 */
  0x0000,               /* D8068 */
  0x0000,               /* D8069 */

  0x01F4,               /* D8070 */
  0x0000,               /* D8071 */
  0x0000,               /* D8072 */
  0x0000,               /* D8073 */
  0x0000,               /* D8074 */
  0x0000,               /* D8075 */
  0x0000,               /* D8076 */
  0x0000,               /* D8077 */
  0x0000,               /* D8078 */
  0x0000,               /* D8079 */

  0x0000,               /* D8080 */
  0x0000,               /* D8081 */
  0x0000,               /* D8082 */
  0x0000,               /* D8083 */
  0x0000,               /* D8084 */
  0x0000,               /* D8085 */
  0x0000,               /* D8086 */
  0x0000,               /* D8087 */
  0x0000,               /* D8088 */
  0x0000,               /* D8089 */

  0x0000,               /* D8090 */
  0x0000,               /* D8091 */
  0x0000,               /* D8092 */
  0x0000,               /* D8093 */
  0x0000,               /* D8094 */
  0x0000,               /* D8095 */
  0x0000,               /* D8096 */
  0x0000,               /* D8097 */
  0x0000,               /* D8098 */
  0x0000,               /* D8099 */

  0x0064,               /* D8100 */
  0x6266,               /* D8101: same model code as D8001 */
  MAXSTEP / 1000,       /* D8102: same capacity as D8002 */
  0x0000,               /* D8103 */
  0x0000,               /* D8104 */
  0x0000,               /* D8105 */
  0x0000,               /* D8106 */
  0x0000,               /* D8107 */
  0x0000,               /* D8108 */
  0x0000,               /* D8109 */

  0x0000,               /* D8110 */
  0x0000,               /* D8111 */
  0x0000,               /* D8112 */
  0x0000,               /* D8113 */
  0x0000,               /* D8114 */
  0x0000,               /* D8115 */
  0x0000,               /* D8116 */
  0x0000,               /* D8117 */
  0x0000,               /* D8118 */
  0x0000,               /* D8119 */

  0x0000,               /* D8120: communication format (bit table in the file header) */
  0x0001,               /* D8121: station (slave) number */
  0x0000,               /* D8122: RS instruction work register */
  0x0000,               /* D8123: RS instruction work register */
  0x0002,               /* D8124: RS header, STX (0x02) */
  0x0003,               /* D8125: RS terminator, ETX (0x03) */
  0x0000,               /* D8126 */
  0x0000,               /* D8127: RS-485 exchange: first address */
  0x0000,               /* D8128: RS-485 exchange: data length */
  0x0000                /* D8129: RS-485 exchange: time-out */
};

/** Upper-case hexadecimal digits used to encode reply frames. */
const char ASCII[16] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0X41, 0X42, 0X43, 0X44, 0X45, 0X46};

/**
 * Convert one ASCII hexadecimal digit (either case) to its value.
 * @return 0..15, or -1 if @p c is not a hexadecimal digit.
 */
static int hex_digit(u8 c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

/**
 * Staging buffer for one flash block. Program downloads (block read-modify-write)
 * and the retained-data backup share it; staging_busy and bctrl1 say who owns it.
 */
static u16 prog_write_buffer[1024];

/** Index of the last byte of the request being received; 0 means idle. STX is stored at index 2. */
static u16 rx_count;
/** Number of bytes of the pending reply in tx_data[]. */
static volatile u16 tx_count;
/** Reply frame; the DMA channel reads it (see App/main.c). */
char tx_data[144];
/** Request frame; index 0 holds the byte just received, the frame itself starts at index 2. */
char rx_data[144];
/** Physical start address of the memory read that UARTSend() encodes. */
static u32 addr;

/** Address operand of the current command, and the result of the last E41 search. */
static u16 prog_address, data_address;

/** 1 while the DMA channel is idle, i.e. the previous reply has been sent completely. */
static volatile u8 UART_Sended = 1;
/** 1 while PLC_DATA_KEEP()/PLC_DATA_RECOVER() own prog_write_buffer; received frames are dropped. */
static volatile u8 staging_busy;
/**
 * data_size: length operand of the current command.
 * bctrl0:    flash block addressed by the program word being stored.
 * bctrl1:    flash block currently held in prog_write_buffer (255 = none).
 */
static u8 data_size, bctrl0, bctrl1;
/** Set when a reply is ready in tx_data[]; the main loop then calls TX_Process(). */
volatile u8 UART_SEND_READY;
/** 1 while a program download is in progress; the runtime holds the PLC in STOP. */
volatile u8 PLC_UPDATE_PROG = 0;

/**
 * Load the factory special registers D8000..D8129 and put the relay image in
 * the STOP state (M8001 and M8003 set). Called once at boot, before the
 * serial port is enabled.
 */
void PLC_DATA_INIT(void) {
  u32 temp;

  for (temp = 0; temp < 130; temp++) {
    PLC_16BIT(Addr_D8000 + temp * 2) = special_d[temp];
  }

  PLC_8BIT(Addr_M8) = 0x0A; /* Boot in STOP (M8001/M8003), also after an interrupted download. */
  bctrl0 = 255;
  bctrl1 = 255;
}

/**
 * Translate a legacy block number into a flash address.
 * Numbers 0..PLC_PROGRAM_BLOCKS-1 are program blocks; 10 is the legacy
 * identifier of the retained-data block.
 * @return true and the block address in @p address, or false for an invalid number.
 */
static bool block_address(u32 number, u32 *address) {
  if (number == 10U) { /* Legacy retention block identifier, not a Flash offset. */
    *address = PLC_RETAIN_BASE;
    return true;
  }
  if (number >= PLC_PROGRAM_BLOCKS) return false;
  *address = ROM_ADDR + number * PLC_FLASH_BLOCK_SIZE;
  return true;
}

/** Erase flash block @p number and program it from prog_write_buffer. */
bool write_block(u32 number) {
  u32 address;
  return block_address(number, &address) &&
         plc_flash_write_block(address, prog_write_buffer);
}

/** Load flash block @p number into prog_write_buffer (start of a read-modify-write). */
bool backup_block(u32 number) {
  u32 address, i;
  if (!block_address(number, &address)) return false;
  for (i = 0; i < PLC_FLASH_BLOCK_SIZE / 2U; i++)
    prog_write_buffer[i] = *(volatile const u16 *)(address + i * 2U);
  return true;
}

/**
 * Write the staged program block (bctrl1) to flash, if there is one, and mark
 * the staging buffer free.
 *
 * Retention and program downloads share one staging buffer. A dirty program
 * block is never replaced by retention data, not even after a failed flush.
 * @return false if the flash write failed (the block stays staged).
 */
static bool flush_program(void) {
  if (bctrl1 != 255U && !write_block(bctrl1)) return false;
  bctrl0 = bctrl1 = 255U;
  return true;
}

/**
 * CRC-16 (reflected polynomial 0xA001, initial value 0xFFFF) over the first
 * 1022 words of prog_write_buffer, i.e. everything before the CRC word.
 */
static u16 retained_crc(void) {
  u32 i, bit;
  u16 crc = 0xFFFFU;
  const u8 *bytes = (const u8 *)prog_write_buffer;
  for (i = 0; i < 1022U * 2U; i++) {
    crc ^= bytes[i];
    for (bit = 0; bit < 8U; bit++)
      crc = (crc & 1U) ? (crc >> 1) ^ 0xA001U : crc >> 1;
  }
  return crc;
}

/**
 * Back up the latched devices to the retained flash block (power-off hook).
 *
 * Writes D500..D949, C100..C149, T100..T149 and M512..M1023, then the CRC and
 * finally the validity marker (layout in the file header). Any program block
 * that is still staged is written first; if that or the backup fails, M8063 is
 * set.
 */
void PLC_DATA_KEEP(void) {
  u32 temp = 0, temp_addr;

  staging_busy = 1;
  if (!flush_program()) { PLC_BIT_ON(M8063); staging_busy = 0; return; }
  for (temp = 0; temp < 1024U; temp++) prog_write_buffer[temp] = 0xFFFFU;
  temp = 0;
  for (temp_addr = Addr_D0000 + 1000; temp_addr < Addr_D0000 + 1900; temp_addr += 2)  /* D500..D949 (450 words) */
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  for (temp_addr = Addr_CVal + 200; temp_addr < Addr_CVal + 300; temp_addr += 2)      /* C100..C149 (50 words) */
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  for (temp_addr = Addr_TVal + 200; temp_addr < Addr_TVal + 300; temp_addr += 2)      /* T100..T149 (50 words) */
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  for (temp_addr = 0x0040; temp_addr < 0x0080; temp_addr += 2)                        /* M512..M1023 (32 words) */
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  prog_write_buffer[1022] = retained_crc();
  prog_write_buffer[1023] = 0xA55AU; /* Written last; interrupted updates stay invalid. */
  if (!write_block(10)) PLC_BIT_ON(M8063);
  staging_busy = 0;
}

/**
 * Restore the latched devices from the retained flash block (power-on hook).
 *
 * The block is used only if its marker (0xA55A) and CRC are valid; otherwise
 * the registers are left untouched. A still-staged program block is flushed
 * first (M8063 is set if that fails).
 */
void PLC_DATA_RECOVER(void) {
  u32 temp = 0, temp_addr;

  staging_busy = 1;
  if (!flush_program()) { PLC_BIT_ON(M8063); staging_busy = 0; return; }
  if (!backup_block(10) || prog_write_buffer[1023] != 0xA55AU ||
      prog_write_buffer[1022] != retained_crc()) { staging_busy = 0; return; }

  for (temp_addr = Addr_D0000 + 1000; temp_addr < Addr_D0000 + 1900; temp_addr += 2)  /* D500..D949 */
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;

  for (temp_addr = Addr_CVal + 200; temp_addr < Addr_CVal + 300; temp_addr += 2)      /* C100..C149 */
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;

  for (temp_addr = Addr_TVal + 200; temp_addr < Addr_TVal + 300; temp_addr += 2)      /* T100..T149 */
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;

  for (temp_addr = 0x0040; temp_addr < 0x0080; temp_addr += 2)                        /* M512..M1023 */
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;
  staging_busy = 0;
}

/**
 * Search the program area for a 16-bit word (command E41).
 *
 * @param start  start address in program space (0x8000 + byte offset, even).
 * @param word   word to look for.
 * @return the program-space address of the first match; 0x8000 plus the program
 *         area size if the word does not occur; 0xFFFF if @p start is not a valid
 *         program-space address.
 */
static u16 find_data(u32 start, u32 word) {
  u8 find_ok;

  find_ok = 5;
  if (start < 0x8000U) return 0xFFFFU;
  start -= 0x8000U;
  if (start >= PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE || (start & 1U)) return 0xFFFFU;
  do {
    if ((PLC_ROM_16BIT(start) == word))
      find_ok = 0;
    else
      start += 2;
    if (start > PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE - 2U)
      find_ok = 1;
  } while (find_ok > 3);
  start += 0X8000;
  return start;
}

/**
 * Run the E41 search and build its reply frame: STX, '1', the four hex digits
 * of the result (high byte first), ETX and the checksum digits.
 *
 * @param data_L  low byte of the word to find.
 * @param data_H  high byte of the word to find.
 */
static void find_data_address(u8 data_L, u8 data_H) {
  u32 temp_sum;

  data_address = find_data(prog_address, (data_H << 8) + data_L);
  tx_data[0] = 0x02;
  temp_sum = 0x31;
  tx_data[1] = temp_sum;
  data_H = data_address >> 8;
  data_L = data_address & 0xFF;
  tx_data[2] = ASCII[data_H >> 4];
  tx_data[3] = ASCII[data_H & 0x0F];
  tx_data[4] = ASCII[data_L >> 4];
  tx_data[5] = ASCII[data_L & 0x0F];
  tx_data[6] = 0X03;
  temp_sum += tx_data[2];
  temp_sum += tx_data[3];
  temp_sum += tx_data[4];
  temp_sum += tx_data[5];
  temp_sum += tx_data[6];
  tx_data[7] = ASCII[(temp_sum & 0xFFU) >> 4];
  tx_data[8] = ASCII[temp_sum & 0x0F];
  tx_count = 9;
}

/**
 * Build a data reply for a memory read: copy data_size bytes starting at the
 * physical address addr into tx_data[] as STX, hex pairs, ETX and checksum.
 *
 * The source must lie completely inside the PLC RAM image or inside the program
 * flash (below the retained block), and the reply must fit the buffer; otherwise
 * the reply is a single NAK.
 */
void UARTSend(void) {
  u32 temp;
  u8 temp_sum;
  u32 data;

  if (data_size > (sizeof(tx_data) - 4U) / 2U ||
      !((addr >= RAM_ADDR && plc_ram_range(addr - RAM_ADDR, data_size)) ||
        (addr >= ROM_ADDR && addr <= PLC_RETAIN_BASE && data_size <= PLC_RETAIN_BASE - addr))) {
    tx_data[0] = 0x15;
    tx_count = 1;
    return;
  }
  tx_data[0] = 0x02;
  temp_sum = 0;
  for (temp = 0; temp < data_size; temp++) {
    data = PLC_Bu(addr + temp);
    tx_data[temp * 2 + 1] = ASCII[data >> 4];
    tx_data[temp * 2 + 2] = ASCII[data & 0x0F];
    temp_sum += tx_data[temp * 2 + 1] + tx_data[temp * 2 + 2];
  }
  tx_data[temp * 2 + 1] = 0x03;
  temp_sum += 0x03;
  tx_data[temp * 2 + 2] = ASCII[temp_sum >> 4];
  tx_data[temp * 2 + 3] = ASCII[temp_sum & 0x0F];
  tx_count = temp * 2 + 4;
}

/**
 * Assemble the monitor snapshot that the host reads with E00 at address 0x1790.
 *
 * The host first writes a descriptor list into RAM: the number of words at
 * offset 0x1400, the number of bits at 0x1402, and from 0x1404 one 16-bit
 * entry per monitored item (the words first, then the bits). Word entries are
 * byte addresses, bit entries are bit addresses. The current values are packed
 * from 0x1790: the words in order (a 32-bit counter in the C200 range counts as
 * two words), followed by the bits, 16 per word.
 *
 * The whole descriptor list is validated before anything is written.
 * @return false if the list is malformed or the result would not fit.
 */
static bool build_monitor(void) {
  u32 words = PLC_8BIT(0x1400), bits = PLC_8BIT(0x1402);
  u32 i, source, output_words = 0, offset;
  if ((words + bits) * 2U > 0x1790U - 0x1404U) return false;
  for (i = 0; i < words; i++) {
    source = PLC_16BIT(0x1404 + i * 2U);
    if ((source & 1U) || !plc_ram_range(source, 2U)) return false;
    output_words++;
    if (source >= Addr_CHVal && source < Addr_CHVal + 56U * 4U) {
      if (!plc_ram_range(source, 4U)) return false;
      output_words++;
    }
  }
  for (i = 0; i < bits; i++) {
    source = PLC_16BIT(0x1404 + (words + i) * 2U);
    if (source >= PLC_RAM_SIZE * 8U) return false;
  }
  /* Monitoring workspace ends at 0x1800, before the user data area. */
  if ((output_words + (bits + 15U) / 16U) * 2U > 0x1800U - 0x1790U) return false;
  offset = 0x1790U;
  for (i = 0; i < words; i++) {
    source = PLC_16BIT(0x1404 + i * 2U);
    PLC_16BIT(offset) = PLC_16BIT(source);
    offset += 2U;
    if (source >= Addr_CHVal && source < Addr_CHVal + 56U * 4U) {
      PLC_16BIT(offset) = PLC_16BIT(source + 2U);
      offset += 2U;
    }
  }
  for (i = 0; i < (bits + 15U) / 16U; i++)
    PLC_16BIT(offset + i * 2U) = 0;
  for (i = 0; i < bits; i++) {
    source = PLC_16BIT(0x1404 + (words + i) * 2U);
    if (PLC_8BIT((source >> 3)) & (1U << (source & 7U)))
      PLC_16BIT(offset + (i >> 4) * 2U) |= 1U << (i & 15U);
  }
  return true;
}

/**
 * May a remote write cover the byte range [offset, offset + length) of the RAM image?
 *
 * M8000..M8007 share one byte that the main loop updates. A read-modify-write
 * from the interrupt would lose queued RUN/STOP or scan-status transitions,
 * even when only another bit of that byte is meant, so the byte is protected.
 */
static bool writable_ram(u32 offset, u32 length) {
  return plc_ram_range(offset, length) &&
         !(length && offset <= Addr_M8 && offset + length > Addr_M8);
}

/** May a remote command set or reset the bit with this bit address? (Same rule as writable_ram().) */
static bool writable_bit(u32 bit) {
  return plc_ram_range(bit >> 3, 1U) && (bit >> 3) != Addr_M8;
}

/**
 * Execute one complete request frame and prepare the reply.
 *
 * Called from RX_Process() after the checksum has been verified. The frame is
 * in rx_data[] (rx_count is the index of its last byte). On return tx_data[] and
 * tx_count hold the reply (ACK, NAK or a data frame), rx_count is cleared and
 * UART_SEND_READY is set so that the main loop transmits the reply.
 *
 * If a reply is still pending or in flight, or the staging buffer is busy, the
 * frame is dropped without any reply.
 */
void Process_switch(void) {
  u32 temp, decoded_count;
  u8 payload[sizeof(rx_data) / 2U];

  /* rx_count is the LAST INDEX: STX at 2, command at 3, ETX at
   * rx_count-2. The special E7/E8 form has FIVE hex digits, not pairs. */
  if (!UART_Sended || UART_SEND_READY || staging_busy) { rx_count = 0; return; }
  if (rx_count < 6U || rx_count >= sizeof(rx_data) ||
      rx_data[rx_count - 2U] != 0x03) goto reject;

  /* E7/E8: force a bit ON/OFF. Four hex digits, low byte first, not pairs. */
  if (rx_data[3] == 'E' && rx_count == 11U &&
      (rx_data[4] == '7' || rx_data[4] == '8')) {
    int digits[4];
    for (temp = 0; temp < 4U; temp++) {
      digits[temp] = hex_digit((u8)rx_data[5U + temp]);
      if (digits[temp] < 0) goto reject;
    }
    prog_address = (digits[0] << 4) | digits[1] |
                   (digits[2] << 12) | (digits[3] << 8);
    if (!writable_bit(prog_address)) goto reject;
    if (rx_data[4] == '8') PLC_BIT_OFF(prog_address);
    else switch (prog_address) {
      case 0x0E23: break;                     /* M8035: forced-run mode flag, ignored */
      case 0x0E24:                            /* M8036: RUN request */
        /* RUN may not execute a half-staged program or hide a failed flush. */
        if (!flush_program()) goto reject;
        PLC_UPDATE_PROG = 0;
        PLC_Runtime_RequestRun(1); break;
      case 0x0E25: PLC_Runtime_RequestRun(0); break;  /* M8037: STOP request */
      default: PLC_BIT_ON(prog_address); break;
    }
    tx_data[0] = 0x06;
    tx_count = 1;
    goto finish;
  }

  /* Decode the payload (hex digit pairs between the command and ETX). */
  if ((rx_count - 6U) & 1U) goto reject;
  decoded_count = (rx_count - 6U) / 2U;
  for (temp = 0; temp < decoded_count; temp++) {
    u32 index = 4U + temp * 2U;
    int hi = hex_digit((u8)rx_data[index]);
    int lo = hex_digit((u8)rx_data[index + 1U]);
    if (hi < 0 || lo < 0) goto reject;
    payload[temp] = (u8)((hi << 4) | lo);
  }

  /* Check the payload length of the command before touching anything. */
  switch (rx_data[3]) {
    case '0': if (decoded_count != 3U) goto reject; break;
    case '1': if (decoded_count < 3U) goto reject; break;
    case '7': case '8': if (decoded_count != 2U) goto reject; break;
    case '4': break; /* Legacy no-op handshake, optional even-length payload. */
    case 'B': if (decoded_count != 0U) goto reject; break;
    case 'E': if (decoded_count < 1U) goto reject; break;
    default: goto reject;
  }

  tx_data[0] = 0x06;   /* default reply: ACK */
  tx_count = 1;

  prog_address = decoded_count >= 2U ? ((u32)payload[0] << 8) | payload[1] : 0U;
  data_size = decoded_count >= 3U ? payload[2] : 0U;

  switch (rx_data[3]) {
    case 0x30:  /* '0': read RAM */
      addr = RAM_ADDR + prog_address;
      UARTSend();

      break;

    case 0x31:  /* '1': write RAM */
      if (!writable_ram(prog_address, data_size) || decoded_count != 3U + data_size) goto reject;
      for (temp = 0; temp < data_size; temp++) {
        PLC_8BIT(temp + prog_address) = payload[3 + temp];
      }
      break;

    case 0x34:  /* '4': legacy handshake; both frame lengths are simply acknowledged */
      if (rx_count == 14) {

        tx_data[0] = 0x06, tx_count = 1;
      } else
        tx_data[0] = 0x06, tx_count = 1;
      break;

    case 0x37:  /* '7': set bit */
      if (!writable_bit(prog_address)) goto reject;
      PLC_BIT_ON(prog_address);
      break;

    case 0x38:  /* '8': reset bit */
      if (!writable_bit(prog_address)) goto reject;
      PLC_BIT_OFF(prog_address);
      break;

    case 0x42:  /* 'B': write the staged program block to flash */
      if (!flush_program()) goto reject;
      break;

    case 0x45:  /* 'E': extended commands, payload[0] is the sub-command */
      switch (payload[0]) {
        case 0x00: case 0x01:
          if (decoded_count != 4U) goto reject;
          break;
        case 0x10: case 0x11: if (decoded_count < 4U) goto reject; break;
        case 0x41: if (decoded_count != 5U) goto reject; break;
        case 0x42: case 0x60: case 0x61: case 0x62: case 0x63: case 0x64:
          if (decoded_count != 1U) goto reject;
          break;
        default: goto reject;
      }
      data_size = decoded_count >= 4U ? payload[3] : 0U;
      prog_address = decoded_count >= 3U ? ((u32)payload[1] << 8) | payload[2] : 0U;
      {
        switch (payload[0]) {
          case 0x00: {
            /* E00: read RAM ("PC_READ_Parameter") */
            if (prog_address == 0x1790U && !build_monitor()) goto reject;

            if (prog_address > 0x3FFF)  /* D0000 is addressed as 0x4000, stored at RAM offset 0x2000 */
              prog_address -= 0x2000;

            addr = RAM_ADDR + prog_address;
            UARTSend();
            break;
          }
          case 0x10:
            /* E10: write RAM ("PC_WRITE_Parameter") */
            if (decoded_count != 4U + data_size || prog_address >= 0x8000U) goto reject;
            if (prog_address < 0x8000) {  /* always true here: 0x8000 and above were rejected */

              if (prog_address > 0x3FFF)
                prog_address -= 0x2000;

              if (!writable_ram(prog_address, data_size) || decoded_count < 4U + data_size) goto reject;
              for (temp = 0; temp < data_size; temp++)
                PLC_8BIT(prog_address + temp) = payload[4 + temp];
            }
            break;

          case 0x01:
            /* E01: read program memory ("PC_READ_PROG") */
            if (prog_address >= 0x8000U) prog_address -= 0x8000U;
            if (prog_address >= PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE) goto reject;
            addr = ROM_ADDR + prog_address;
            UARTSend();
            break;

          case 0x11:
            /* E11: write program memory ("PC_WRITE_PROG") */
            if (decoded_count != 4U + data_size || data_size == 0U) goto reject;
            if (prog_address >= 0x8000U) {
              /* Program words: stage them in the block buffer, one flash block at a time. */
              prog_address -= 0x8000U;
              if ((prog_address & 1U) || (data_size & 1U) ||
                  prog_address >= PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE ||
                  data_size > PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE - prog_address) goto reject;
              PLC_Runtime_RequestRun(0);
              PLC_UPDATE_PROG = 1;
              PLC_NEW_PROG = 1;
              for (temp = 0; temp < data_size; temp += 2U) {
                u32 offset = prog_address + temp;
                bctrl0 = offset / PLC_FLASH_BLOCK_SIZE;
                if (bctrl0 != bctrl1) {
                  /* Moving to another block: write the finished one, load the next. */
                  if (bctrl1 != 255U && !write_block(bctrl1)) goto reject;
                  if (!backup_block(bctrl0)) goto reject;
                  bctrl1 = bctrl0;
                }
                prog_write_buffer[(offset % PLC_FLASH_BLOCK_SIZE) / 2U] =
                  payload[4 + temp] | ((u16)payload[5 + temp] << 8);
              }
            } else {
              /* Below 0x8000 the data goes to RAM instead of the program. */
              if (!writable_ram(prog_address, data_size)) goto reject;
              PLC_Runtime_RequestRun(0);
              PLC_UPDATE_PROG = 1;
              for (temp = 0; temp < data_size; temp++)
                PLC_8BIT(temp + prog_address) = payload[4 + temp];
            }
            break;

          case 0x41:  /* E41: search the program for a word */
            find_data_address(payload[3], payload[4]);
            break;

          case 0x42:  /* E42: write the staged program block to flash */
            if (!flush_program()) goto reject;
            break;

          case 0x61:  /* E61: discard the staged program block */
            bctrl1 = 255;
            bctrl0 = 255;
            break;

          case 0x60:
            /* E60: clear program memory (parameter header plus a lone END instruction) */
            PLC_Runtime_RequestRun(0);
            PLC_UPDATE_PROG = PLC_NEW_PROG = 1;
            /* Clearing explicitly discards the pending download. */
            bctrl0 = bctrl1 = 255U;
            if (!backup_block(0)) goto reject;
            prog_write_buffer[46] = 0x000F;  /* END instruction at byte offset 0x5C */
            for (temp = 47; temp < 1024; temp++)
              prog_write_buffer[temp] = 0xFFFF;
            if (!write_block(0)) goto reject;
            for (temp = 1; temp < PLC_PROGRAM_BLOCKS; temp++)
              if (!plc_flash_erase_block(ROM_ADDR + temp * PLC_FLASH_BLOCK_SIZE)) goto reject;
            bctrl0 = bctrl1 = 255U;
            break;

          case 0x62:
            /* E62: clear M0..M3071 */
            for (temp = Addr_ML; temp < Addr_ML + 3072 / 8; temp += 2)
              PLC_16BIT(temp) = 0x00;
            break;

          case 0x63:
            /* E63: clear D0..D1999 (MAXDXXX words) */
            for (temp = Addr_D0000; temp < Addr_D0000 + MAXDXXX * 2; temp += 2)
              PLC_16BIT(temp) = 0x00;
            break;

          case 0x64:
            /* E64: clear D0..D999 (the FX1N general data registers) */
            for (temp = Addr_D0000; temp < Addr_D0000 + 1000 * 2; temp += 2)
              PLC_16BIT(temp) = 0x00;
            break;

          default:
            tx_data[0] = 0x15;
            tx_count = 1;
            break;
        }
      }
      break;

    default:
      tx_data[0] = 0x15;
      tx_count = 1;
      break;
  }
  goto finish;
reject:
  tx_data[0] = 0x15;  /* NAK */
  tx_count = 1;
finish:
  rx_count = 0;
  UART_SEND_READY = 1;
}

/**
 * Set the USART1 baud rate and enable the receiver, transmitter and receive
 * interrupt. The firmware itself configures 19200 baud in App/main.c; this
 * helper is for changing the rate later and is exercised by the host tests.
 * @param baud  bits per second; 0 is ignored.
 */
void usart(u16 baud) {
  if (baud == 0U) return;
  USART1->BRR = (board_apb2_hz() + baud / 2U) / baud;
  USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
}

/** DMA1 channel 4 (USART1 TX) interrupt: the reply has been sent, or the transfer failed. */
void DMA1_Channel4_IRQHandler(void) {
  u32 status = DMA1->ISR;
  if (status & (DMA_ISR_TCIF4 | DMA_ISR_TEIF4)) {
    DMA1_Channel4->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF4; /* Write-one-to-clear, never read/modify IFCR. */
    tx_count = 0;
    UART_Sended = 1;
    if (status & DMA_ISR_TEIF4) PLC_BIT_ON(M8063);  /* transfer error */
  }
}

/**
 * Start the DMA transmission of the prepared reply (main loop, when
 * UART_SEND_READY is set). Does nothing while the previous transfer is running
 * or when there is nothing to send.
 */
void TX_Process(void) {
  if (UART_Sended && tx_count > 0U && tx_count <= sizeof(tx_data)) {
    UART_SEND_READY = UART_Sended = 0;
    DMA1_Channel4->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF4;
    DMA1_Channel4->CNDTR = tx_count;
    __DMB();
    DMA1_Channel4->CCR |= DMA_CCR_EN;
  }
}

/**
 * Receive one byte (USART1 interrupt) and advance the frame state machine.
 *
 * ENQ is answered with ACK at once. STX (re)starts a frame; bytes are then
 * collected in rx_data[] until ETX and the two checksum digits have arrived.
 * A frame with a valid checksum goes to Process_switch(); a wrong checksum or
 * an oversized frame is answered with NAK. Bytes outside a frame are ignored.
 *
 * The DMA source buffer must stay unchanged until the transfer completes, so
 * while a reply is pending or in flight (or the staging buffer is busy) the
 * byte is consumed and discarded; a new STX is needed afterwards.
 */
void RX_Process(void) {
  u32 sum;
  u32 temp;

  /* Always consume DR, even while dropping input. The DMA source must stay
   * immutable until completion; a response waiting for main also owns it.
   * Busy input is dropped, not queued. A new STX is required afterwards. */
  temp = 0xFFU & USART1->DR;
  if (!UART_Sended || UART_SEND_READY || staging_busy) { rx_count = 0; return; }
  rx_data[0] = (char)temp;

  if (rx_data[0] == 0X05) {
    /* ENQ: link test */
    rx_count = 0;
    tx_data[0] = 0x06;
    tx_count = 1;
    TX_Process();
    return;
  } else if (rx_data[0] == 0X02) {
    /* STX: start (or restart) a frame */
    rx_count = 0x01;
  } else if (rx_count == 0) {
    /* Noise is not a baud-negotiation command. Keep the configured 19200 8N1. */
    return;
  }

  if (rx_count > 0) {
    if (rx_count >= sizeof(rx_data) - 1U) {
      /* Frame too long for the buffer. */
      rx_count = 0;
      tx_data[0] = 0x15;
      tx_count = 1;
      UART_SEND_READY = 1;
      return;
    }
    rx_count++;
    rx_data[rx_count] = rx_data[0];
    /* ETX seen two bytes ago: the last two bytes are the checksum digits. */
    if ((rx_count > 3) && (rx_data[rx_count - 2] == 0x03)) {
      sum = 0;
      for (temp = 3; temp < (rx_count - 1U); temp++)
        sum += rx_data[temp];

      int hi = hex_digit((u8)rx_data[rx_count - 1]);
      int lo = hex_digit((u8)rx_data[rx_count]);
      temp = hi >= 0 && lo >= 0 ? ((u32)hi << 4) | (u32)lo : 256U;
      if (hi >= 0 && lo >= 0 && (sum & 0xFFU) == temp) {
        Process_switch();

      } else {
        /* Checksum mismatch or non-hex checksum digits. */
        rx_count = 0;
        tx_data[0] = 0x15;
        tx_count = 1;
        TX_Process();
      }
    }
  }
}

/**
 * USART1 interrupt. Receive errors (overrun, framing, noise, parity) abandon
 * the frame in progress; a received byte is handed to RX_Process().
 */
void USART1_IRQHandler(void) {
  u32 status = USART1->SR;
  if (status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
    (void)USART1->DR;
    rx_count = 0; /* A damaged byte cannot participate in a valid packet. */
  } else if (status & USART_SR_RXNE) {
    RX_Process(); /* Read DR after SR: also clears receive error flags. */
  }
}
