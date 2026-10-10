/********************************************************/
// Register-only port targets STM32F103C8 BluePill (20 KB RAM, 64 KB Flash).
// The original implementation was tested on STM32F103RDT6 and VET6.
// Edit date: 20150909
// editor by Xiao Xiaoying
// Online shop: shop182385147.taobao.com
/********************************************************
PLC related special register
Dedicated auxiliary relay
M8126 global flag
M8127 communication request handshake signal
M8128 error flag
M8129 communication request switching
The next original annotation was unreadable.
Dedicated data register description
D8000 = 200; scan time
D8001 = 0X5EF6; Model version FX2N(C)
D8101 = 0X5EF6; Model version FX2N(C)
D8002 = 8; memory capacity
D8102 = 8; memory capacity
D8003 = 0x0010; memory type, register type
D8006 CPU battery voltage
D8010 = 10; scan current value
D8011 = 20; scan minimum time (0.1MS)
D8012 = 140; maximum scan time (0.1MS)
D6030 D6031 D6032 D6033 is an analog input
D8080 D8081 is the analog output

D8120 = 0X4096 communication format
D8121 slave number (up to 16)
D8127 The first address of the exchange data
D8128 exchange data volume
D8129 Network communication timeout time confirmation value
D8000 watchdog
D8019 corresponds to the week
D8018 corresponding year
D8017 corresponding month
D8016 corresponding date
D8015 corresponding hour
D8014 corresponds to the minute
D8013 corresponds to the second
Detailed communication format (D8120)
-------------------------------------------------- --------------------
Bit number | Meaning | Description
-----------+-------------+------------------------ --------------------
B0         | data length | 0: 7 digits 1: 8 digits
-----------+-------------+------------------------ --------------------
B2b1       |Verification | 
           | method      | 00: No 01: Odd parity 11: Even parity
-----------+-------------+------------------------ --------------------
B3         | Stop Bit    | 0: 1 bit 1: 2 bits
-----------+-------------+------------------------ --------------------
           |             | 0001:300 0111:4800
B7b6b5b4   | Baud Rate   | 0100:600 1000:9600
           |             | 0101:1200 1001:19200
           |             | 0110:2400
-----------+-------------+------------------------ --------------------
B8         |             | 0: No Note: No protocol communication
-----------+-------------+------------------------ --------------------
B9         |             | 0: not the same as above
-----------+-------------+------------------------ --------------------
B12b11b10  |Communication|
           | Interface   | 000: RS485 (RS422) Interface
           |             | 010: (RS232) interface
-----------+-------------+------------------------ --------------------
B13        | Summation   |
           | check       | 0: No summation code 1: Automatic summation code
-----------+-------------+------------------------ -------------------
B14        | Protocol    | 0: No protocol communication 1: Dedicated communication protocol
-----------+-------------+------------------------ --------------------
B15        | Protocol    |
           | format      | 0: Format 1 1: Format 4
-------------------------------------------------- --------------------
Example: D8120 = 0X4096 Communication baud rate is 19200
*********************************************************************************/

#include "platform.h"
#include "plc_flash.h"
#include "plc_memory.h"
#include "plc_runtime.h"
#include "board_clock.h"

#include <stdio.h>
#include "PLC_Dialogue.h"
#include "plc_io.h"
#include "PLC_CONF.H"

extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

#ifdef __ICCARM__
#pragma location=".plc_rom"
__root const u8 PLC_ROM[110] = {
#else
const u8 PLC_ROM[110] __attribute__((section(".plc_rom"), used)) = {
#endif
// The starting address of FLASH is PLC information **************************
// Legacy parameter capacity is in thousands of 16-bit instruction words.
MAXSTEP / 1000,0x00, // 00 0x0000 -- configured capacity, not commercial PLC compatibility
0x00,0x00, // 02        -- 
0x00,0x00, // 04
0x00,0x00, // 06

0x20,0x20, // 08 0x0008 -- PASSWORD 0
0x20,0x20, // 10        -- PASSWORD 1
0x20,0x20, // 12        -- PASSWORD 2
0x20,0x20, // 14        -- PASSWORD 3

0x20,0x20, // 16 0x0010 -- PLC NAME 0
0x20,0x20, // 18        -- PLC NAME 1
0x20,0x20, // 20        -- PLC NAME 2
0x20,0x20, // 22        -- PLC NAME 3
0x20,0x20, // 24        -- PLC NAME 4
0x20,0x20, // 26        -- PLC NAME 5
0x20,0x20, // 28        -- PLC NAME 6
0x20,0x20, // 30        -- PLC NAME 7
0x20,0x20, // 32        -- PLC NAME 8
0x20,0x20, // 34        -- PLC NAME 9
0x20,0x20, // 36        -- PLC NAME A
0x20,0x20, // 38        -- PLC NAME B
0x20,0x20, // 40        -- PLC NAME C
0x20,0x20, // 42        -- PLC NAME D
0x20,0x20, // 44        -- PLC NAME E
0x20,0x20, // 46        -- PLC NAME F

0x00,0x00, // 48 0x0030 -- M   Latch Start /
0x00,0x00, // 50        -- M   Latch End   /
0x00,0x00, // 52        -- S   Latch Start /        
0x00,0x00, // 54        -- S   Latch End   /       
0x00,0x0E, // 56        -- C16 Latch Start /
0x00,0x0E, // 58        -- C16 Latch End   /
0xC8,0x0E, // 60        -- C32 Latch Start /
0xFF,0x0E, // 62        -- C32 Latch End   /

0x00,0x00, // 64 0x0040 -- D   Latch Start /
0x00,0x00, // 66        -- D   Latch End   /
0x00,0x00, // 68        --
0x00,0x00, // 70        --
0x00,0x00, // 72        -- File register address
0x00,0x00, // 74        -- File register capaity blocks
0x00,0x00, // 76        -- Comments address
0x00,0x00, // 78        -- Comments capaity blocks

0x00,0x00, // 80 0x0050 -- 0x80 (RUN X007 -- X000) 0x40 -- PLC system 1/2
0x00,0x00, // 82        --
0x00,0x00, // 84        --
0x00,0x00, // 86        --
0x00,0x00, // 88        --
0x00,0x00, // 90        --

0x0F,0x00, // 92 0x005C -- PROGRAM START  End instruction
0xFF,0xFF, // 94
0xFF,0xFF, // 96
0xFF,0xFF, // 98
0xFF,0xFF};
  
const u16 special_d[130] = {
0x00C8,  // D8000 - WDT 200mc
0x6266,  // D8001 - inherited FX1N model code; GX compatibility is unverified
MAXSTEP / 1000,  // D8002 - configured capacity in thousands of words
0x0000,  // D8003 - Internal ( 0x10 - CPURAM, 0x00 - RAM, 0x01 - EPROM, 0x02 - EEPROM )
0x0000,  // D8004 - Error
0x0025,  // D8005 - Batary 3.7
0x001E,  // D8006 - Low Batary 3.0
0x0000,  // D8007 - 
0xFFFF,  // D8008 - 
0x0000,  // D8009 - 
0x0000,  // D8010 - 
0x0000,  // D8011 - 
0x0000,  // D8012 - 
0x0000,  // D8013 - Time Sec
0x0000,  // D8014 - Time Min
0x0000,  // D8015 - Time Hour
0x0000,  // D8016 - Time Date
0x0000,  // D8017 - Time Mounth
0x0000,  // D8018 - Time Year
0x0000,  // D8019 - Time Day

0x000A,  // D8020 - 
0x0000,  // D8021 - 
0x0000,  // D8022 - 
0x0000,  // D8023 - 
0x0000,  // D8024 - 
0x0000,  // D8025 - 
0x0000,  // D8026 - 
0x0000,  // D8027 - 
0x0000,  // D8028 - 
0x0000,  // D8029 - 

0x0000,  // D8030 - AN1
0x0000,  // D8031 - AN2
0x0000,  // D8032 - 
0x0000,  // D8033 - 
0x0000,  // D8034 - 
0x0000,  // D8035 - 
0x0000,  // D8036 - 
0x0000,  // D8037 - 
0x0000,  // D8038 - 
0x0000,  // D8039 - 

0xFFFF,  // D8040 - 
0xFFFF,  // D8041 - 
0xFFFF,  // D8042 - 
0xFFFF,  // D8043 - 
0xFFFF,  // D8044 - 
0xFFFF,  // D8045 - 
0xFFFF,  // D8046 - 
0xFFFF,  // D8047 - 
0x0000,  // D8048 - 
0xFFFF,  // D8049 - 
0x0000,  // D8050 - 
0x0000,  // D8051 - 
0x0000,  // D8052 - 
0x0000,  // D8053 - 
0x0000,  // D8054 - 
0x0000,  // D8055 - 
0x0000,  // D8056 - 
0x0000,  // D8057 - 
0x0000,  // D8058 - 
0x0000,  // D8059 - 

0x0000,  // D8060 - 
0x0000,  // D8061 - 
0x183B,  // D8062 -
0x0000,  // D8063 -
0x0000,  // D8064 -
0x0000,  // D8065 -
0x0000,  // D8066 -
0x0000,  // D8067 -
0x0000,  // D8068 -
0x0000,  // D8069 -

0x01F4,  // D8070 -
0x0000,  // D8071 -
0x0000,  // D8072 -
0x0000,  // D8073 -
0x0000,  // D8074 -
0x0000,  // D8075 -
0x0000,  // D8076 -
0x0000,  // D8077 -
0x0000,  // D8078 -
0x0000,  // D8079 -
0x0000,  // D8080 - 
0x0000,  // D8081 - 
0x0000,  // D8082 - 
0x0000,  // D8083 - 
0x0000,  // D8084 - 
0x0000,  // D8085 - 
0x0000,  // D8086 - 
0x0000,  // D8087 - 
0x0000,  // D8088 - 
0x0000,  // D8089 - 
0x0000,  // D8090 - 
0x0000,  // D8091 - 
0x0000,  // D8092 - 
0x0000,  // D8093 - 
0x0000,  // D8094 - 
0x0000,  // D8095 - 
0x0000,  // D8096 - 
0x0000,  // D8097 - 
0x0000,  // D8098 - 
0x0000,  // D8099 - 

0x0064,  // D8100 - 
0x6266,  // D8101 - same legacy identity as D8001
MAXSTEP / 1000, // D8102 - same configured capacity as D8002
0x0000,  // D8103 - 
0x0000,  // D8104 - 
0x0000,  // D8105 - 
0x0000,  // D8106 - 
0x0000,  // D8107 - 
0x0000,  // D8108 - 
0x0000,  // D8109 - 
0x0000,  // D8110 -
0x0000,  // D8111 -
0x0000,  // D8112 -
0x0000,  // D8113 -
0x0000,  // D8114 -
0x0000,  // D8115 -
0x0000,  // D8116 -
0x0000,  // D8117 -
0x0000,  // D8118 -
0x0000,  // D8119 -

0x0000,  // D8120 - Comunication format
0x0001,  // D8121 - Slave number
0x0000,  // D8122 - RS
0x0000,  // D8123 - RS
0x0002,  // D8124 - RS Header ( STX (02h) )
0x0003,  // D8125 - RS Footer ( ETX (03h) )
0x0000,  // D8126 -
0x0000,  // D8127 - RS485 addr 
0x0000,  // D8128 - RS486 len
0x0000   // D8129 - RS485 timeout
};

const char ASCII[16] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0X41, 0X42, 0X43, 0X44, 0X45, 0X46};
static int hex_digit(u8 c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static u16 prog_write_buffer[1024];

static u16 rx_count;
static volatile u16 tx_count;
char tx_data[144];
char rx_data[144];
static u32 addr;

static u16 prog_address, data_address;

static volatile u8 UART_Sended = 1;
static volatile u8 staging_busy;
static u8 data_size, bctrl0, bctrl1;
volatile u8 UART_SEND_READY;
volatile u8 PLC_UPDATE_PROG = 0;

void PLC_DATA_INIT(void) {
u32 temp;

  for ( temp = 0; temp < 130; temp++ ) {
    PLC_16BIT(Addr_D8000 + temp * 2) = special_d[temp];
  }

  PLC_8BIT(Addr_M8) = 0x0A; // Boot STOP (M8001/M8003), including after interrupted downloads.
  bctrl0 = 255;
  bctrl1 = 255;
}

static bool block_address(u32 number, u32 *address) {
  if (number == 10U) { /* Legacy retention block identifier, not a Flash offset. */
    *address = PLC_RETAIN_BASE;
    return true;
  }
  if (number >= PLC_PROGRAM_BLOCKS) return false;
  *address = ROM_ADDR + number * PLC_FLASH_BLOCK_SIZE;
  return true;
}

bool write_block(u32 number) {
  u32 address;
  return block_address(number, &address) &&
         plc_flash_write_block(address, prog_write_buffer);
}

bool backup_block(u32 number) {
  u32 address, i;
  if (!block_address(number, &address)) return false;
  for (i = 0; i < PLC_FLASH_BLOCK_SIZE / 2U; i++)
    prog_write_buffer[i] = *(volatile const u16 *)(address + i * 2U);
  return true;
}

/* Retention and program downloads share one staging buffer. Never replace a
 * dirty program block with retention data (including on a failed flush). */
static bool flush_program(void) {
  if (bctrl1 != 255U && !write_block(bctrl1)) return false;
  bctrl0 = bctrl1 = 255U;
  return true;
}

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

/***********************************************
  Function: Back up data on power-off
***********************************************/
void PLC_DATA_KEEP(void) {  
u32 temp = 0, temp_addr;

  staging_busy = 1;
  if (!flush_program()) { PLC_BIT_ON(M8063); staging_busy = 0; return; }
  for (temp = 0; temp < 1024U; temp++) prog_write_buffer[temp] = 0xFFFFU;
  temp = 0;
  for ( temp_addr = Addr_D0000 + 1000; temp_addr < Addr_D0000 + 1900; temp_addr +=2 ) // Back up 400 D registers starting at D500
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  for ( temp_addr = Addr_CVal + 200; temp_addr < Addr_CVal + 300; temp_addr +=2 ) // Back up 50 T registers starting at T100
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  for ( temp_addr = Addr_TVal + 200; temp_addr < Addr_TVal + 300; temp_addr +=2 ) // Back up 50 D registers starting at C100
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  for ( temp_addr = 0x0040; temp_addr < 0x0080; temp_addr +=2 )      // Back up M512-M1023
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  prog_write_buffer[1022] = retained_crc();
  prog_write_buffer[1023] = 0xA55AU; /* Written last; interrupted updates stay invalid. */
  if (!write_block(10)) PLC_BIT_ON(M8063);
  staging_busy = 0;
}

/***********************************************
  Function: Restore data on power-on
***********************************************/
void PLC_DATA_RECOVER(void) { 
u32 temp = 0, temp_addr;

  staging_busy = 1;
  if (!flush_program()) { PLC_BIT_ON(M8063); staging_busy = 0; return; }
  if (!backup_block(10) || prog_write_buffer[1023] != 0xA55AU ||
      prog_write_buffer[1022] != retained_crc()) { staging_busy = 0; return; }

  for ( temp_addr = Addr_D0000 + 1000; temp_addr < Addr_D0000 + 1900; temp_addr +=2 )
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;

  for ( temp_addr = Addr_CVal + 200; temp_addr < Addr_CVal + 300; temp_addr +=2 )
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;

  for ( temp_addr = Addr_TVal + 200; temp_addr < Addr_TVal + 300; temp_addr +=2 )
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;

  for ( temp_addr = 0x0040; temp_addr < 0x0080; temp_addr +=2 )
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;
  staging_busy = 0;
}

/*******************************************************************************
Function name: PLC_Comm_Byte
Function: Remap and process communication byte addresses
Input parameter: Cmd command          
Output parameter: Mapped physical address (16-bit address)
********************************************************************************/

/*******************************************************************************
Function name: PLC_Com_BIT
Function: Remap and process communication bit addresses
Input parameter: Cmd command          
Output parameter: Mapped physical address (bit address)
********************************************************************************/

static u16 find_data(u32 addr,u32 find_data) {
u8 find_ok;

  find_ok = 5;
  if (addr < 0x8000U) return 0xFFFFU;
  addr -= 0x8000U;
  if (addr >= PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE || (addr & 1U)) return 0xFFFFU;
  do {
    if ( ( PLC_ROM_16BIT(addr) == find_data ) )
      find_ok = 0;
    else
      addr += 2;
    if ( addr > PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE - 2U )
      find_ok = 1;
  } while ( find_ok > 3 );
  addr += 0X8000;
  return addr;
}

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

// ----------- !!!!!!!!!!!!!!!!!!!!!!!!!!!! ------------------
// Write 8 bit in 16 bit registr

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
  for ( temp = 0; temp < data_size; temp++ ) { 
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

/* Monitor words are byte addresses; monitor bits are bit addresses. Pack the
 * result as aligned words followed by aligned groups of 16 bits. Validate the
 * entire descriptor list before changing any output. */
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

/* M8000..M8007 share a byte updated by main. IRQ writes/RMW would lose queued
 * RUN/STOP or scan status transitions, even when touching another bit. */
static bool writable_ram(u32 offset, u32 length) {
  return plc_ram_range(offset, length) &&
         !(length && offset <= Addr_M8 && offset + length > Addr_M8);
}

static bool writable_bit(u32 bit) {
  return plc_ram_range(bit >> 3, 1U) && (bit >> 3) != Addr_M8;
}

void Process_switch(void) {
u32 temp, decoded_count;
u8 payload[sizeof(rx_data) / 2U];

  /* rx_count is the LAST INDEX: STX at 2, command at 3, ETX at
   * rx_count-2. The special E7/E8 form has FIVE hex digits, not pairs. */
  if (!UART_Sended || UART_SEND_READY || staging_busy) { rx_count = 0; return; }
  if (rx_count < 6U || rx_count >= sizeof(rx_data) ||
      rx_data[rx_count - 2U] != 0x03) goto reject;
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
      case 0x0E23: break;
      case 0x0E24:
        /* RUN may not execute a half-staged program or hide a failed flush. */
        if (!flush_program()) goto reject;
        PLC_UPDATE_PROG = 0;
        PLC_Runtime_RequestRun(1); break;
      case 0x0E25: PLC_Runtime_RequestRun(0); break;
      default: PLC_BIT_ON(prog_address); break;
    }
    tx_data[0] = 0x06;
    tx_count = 1;
    goto finish;
  }
  if ((rx_count - 6U) & 1U) goto reject;
  decoded_count = (rx_count - 6U) / 2U;
  for (temp = 0; temp < decoded_count; temp++) {
    u32 index = 4U + temp * 2U;
    int hi = hex_digit((u8)rx_data[index]);
    int lo = hex_digit((u8)rx_data[index + 1U]);
    if (hi < 0 || lo < 0) goto reject;
    payload[temp] = (u8)((hi << 4) | lo);
  }

  switch (rx_data[3]) {
    case '0': if (decoded_count != 3U) goto reject; break;
    case '1': if (decoded_count < 3U) goto reject; break;
    case '7': case '8': if (decoded_count != 2U) goto reject; break;
    case '4': break; /* Legacy no-op handshake, optional even-length payload. */
    case 'B': if (decoded_count != 0U) goto reject; break;
    case 'E': if (decoded_count < 1U) goto reject; break;
    default: goto reject;
  }

  tx_data[0] = 0x06;
  tx_count = 1;

  prog_address = decoded_count >= 2U ? ((u32)payload[0] << 8) | payload[1] : 0U;
  data_size = decoded_count >= 3U ? payload[2] : 0U;

  switch (rx_data[3]) { 
    case 0x30: 
      addr = RAM_ADDR + prog_address;
      UARTSend();

      break;

    case 0x31:
      if (!writable_ram(prog_address, data_size) || decoded_count != 3U + data_size) goto reject;
      for ( temp = 0; temp < data_size; temp++ ) {
        PLC_8BIT(temp + prog_address) = payload[3 + temp];
      }
      break;

    case 0x34: 
      if ( rx_count == 14 ) {

        tx_data[0]=0x06, tx_count=1;  
      } else
        tx_data[0]=0x06, tx_count=1; 
      break;

    case 0x37:
      if (!writable_bit(prog_address)) goto reject;
      PLC_BIT_ON(prog_address);
      break;

    case 0x38:
      if (!writable_bit(prog_address)) goto reject;
      PLC_BIT_OFF(prog_address);
      break;

    case 0x42: 
      if (!flush_program()) goto reject;
      break;

    case 0x45: // -- Exx -- PC_OPTION_PROG
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
// -- E00 -- PC_READ_Parameter
            if (prog_address == 0x1790U && !build_monitor()) goto reject;

            if ( prog_address > 0x3FFF ) // D0000
              prog_address -= 0x2000;

            addr = RAM_ADDR + prog_address;
            UARTSend();
            break;
          }
          case 0x10:
// -- E10 -- PC_WRITE_Parameter
            if (decoded_count != 4U + data_size || prog_address >= 0x8000U) goto reject;
            if ( prog_address < 0x8000 ) { // 0xE420 - Comments

              if ( prog_address > 0x3FFF )
                prog_address -= 0x2000;

              if (!writable_ram(prog_address, data_size) || decoded_count < 4U + data_size) goto reject;
              for ( temp = 0; temp < data_size; temp++)
                PLC_8BIT(prog_address + temp) = payload[4 + temp];
            }
            break;

          case 0x01:
// -- E01 -- PC_READ_PROG
            if (prog_address >= 0x8000U) prog_address -= 0x8000U;
            if (prog_address >= PLC_PROGRAM_BLOCKS * PLC_FLASH_BLOCK_SIZE) goto reject;
            addr = ROM_ADDR + prog_address;
            UARTSend();
            break;

          case 0x11:
// -- E11 -- PC_WRITE_PROG
            if (decoded_count != 4U + data_size || data_size == 0U) goto reject;
            if (prog_address >= 0x8000U) {
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
                  if (bctrl1 != 255U && !write_block(bctrl1)) goto reject;
                  if (!backup_block(bctrl0)) goto reject;
                  bctrl1 = bctrl0;
                }
                prog_write_buffer[(offset % PLC_FLASH_BLOCK_SIZE) / 2U] =
                  payload[4 + temp] | ((u16)payload[5 + temp] << 8);
              }
            } else {
              if (!writable_ram(prog_address, data_size)) goto reject;
              PLC_Runtime_RequestRun(0);
              PLC_UPDATE_PROG = 1;
              for (temp = 0; temp < data_size; temp++)
                PLC_8BIT(temp + prog_address) = payload[4 + temp];
            }
            break;

          case 0x41: find_data_address(payload[3], payload[4]); break;

          case 0x42: if (!flush_program()) goto reject; break;

          case 0x61: 
            bctrl1 = 255;
            bctrl0 = 255;
          break;

          case 0x60:
// -- E60 -- Clear flash
            PLC_Runtime_RequestRun(0);
            PLC_UPDATE_PROG = PLC_NEW_PROG = 1;
            /* Clearing explicitly discards the pending download. */
            bctrl0 = bctrl1 = 255U;
            if (!backup_block(0)) goto reject;
            prog_write_buffer[46] = 0x000F;  // 0x5C
            for ( temp = 47; temp < 1024; temp++ )
              prog_write_buffer[temp] = 0xFFFF;
            if (!write_block(0)) goto reject;
            for (temp = 1; temp < PLC_PROGRAM_BLOCKS; temp++)
              if (!plc_flash_erase_block(ROM_ADDR + temp * PLC_FLASH_BLOCK_SIZE)) goto reject;
            bctrl0 = bctrl1 = 255U;
            break;

          case 0x62:
// -- E62 -- Clear M0000 - M3071
            for ( temp = Addr_ML; temp < Addr_ML + 3072 / 8; temp += 2 )
              PLC_16BIT(temp) = 0x00;
            break;

          case 0x63:
// -- E63 -- Clear D0000 - D7999
            for ( temp = Addr_D0000; temp < Addr_D0000 + MAXDXXX * 2; temp += 2 )
              PLC_16BIT(temp) = 0x00;
            break;

          case 0x64:
// -- E64 -- Clear D0000 - D0999 !!!! FX1N !!!!
            for ( temp = Addr_D0000; temp < Addr_D0000 + 1000 * 2; temp += 2 )
              PLC_16BIT(temp) = 0x00;
            break;

//          case 0xD1: online_write_data();  break;  

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
  tx_data[0] = 0x15;
  tx_count = 1;
finish:
  rx_count = 0;
  UART_SEND_READY = 1;
}                 

void usart(u16 baud) {
  if (baud == 0U) return;
  USART1->BRR = (board_apb2_hz() + baud / 2U) / baud;
  USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
}

//---------------------Serial-port configuration---------------------
void DMA1_Channel4_IRQHandler(void) {
  u32 status = DMA1->ISR;
  if (status & (DMA_ISR_TCIF4 | DMA_ISR_TEIF4)) {
    DMA1_Channel4->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF4; /* Write-one-to-clear, never read/modify IFCR. */
    tx_count = 0;
    UART_Sended = 1;
    if (status & DMA_ISR_TEIF4) PLC_BIT_ON(M8063);
  }
}

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

void RX_Process(void) {   
u32 sum;
u32 temp;

  /* Always consume DR, even while dropping input. The DMA source must stay
   * immutable until completion; a response waiting for main also owns it.
   * Busy input is dropped, not queued. A new STX is required afterwards. */
  temp = 0xFFU & USART1->DR;
  if (!UART_Sended || UART_SEND_READY || staging_busy) { rx_count = 0; return; }
  rx_data[0] = (char)temp;

  if ( rx_data[0] == 0X05 ) {
    rx_count = 0;
    tx_data[0] = 0x06;
    tx_count = 1;
    TX_Process();
    return;
  } else if ( rx_data[0] == 0X02 ) {
    rx_count = 0x01;
  } else if ( rx_count == 0 ) {
    /* Noise is not a baud-negotiation command. Keep the configured 19200 8N1. */
    return;
  }

  if ( rx_count > 0 ) {
    if (rx_count >= sizeof(rx_data) - 1U) {
      rx_count = 0;
      tx_data[0] = 0x15;
      tx_count = 1;
      UART_SEND_READY = 1;
      return;
    }
    rx_count++;
    rx_data[rx_count] = rx_data[0];
    if ( ( rx_count > 3 ) && ( rx_data[rx_count - 2] == 0x03 ) ) {
      sum = 0;
      for ( temp = 3; temp < (rx_count - 1U); temp++ )
        sum += rx_data[temp];

      int hi = hex_digit((u8)rx_data[rx_count - 1]);
      int lo = hex_digit((u8)rx_data[rx_count]);
      temp = hi >= 0 && lo >= 0 ? ((u32)hi << 4) | (u32)lo : 256U;
      if (hi >= 0 && lo >= 0 && (sum & 0xFFU) == temp) {
        Process_switch(); 

      } else {
        rx_count = 0;
        tx_data[0] = 0x15;
        tx_count = 1;
        TX_Process();
      }
    }
  }     
}

void USART1_IRQHandler(void) {
  u32 status = USART1->SR;
  if (status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
    (void)USART1->DR;
    rx_count = 0; /* A damaged byte cannot participate in a valid packet. */
  } else if (status & USART_SR_RXNE) {
    RX_Process(); /* Read DR after SR: also clears receive error flags. */
  }
}
