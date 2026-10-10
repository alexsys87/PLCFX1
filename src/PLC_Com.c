/********************************************************/
// CPU needs: STM32F103--RAM memory is not less than 64K Flash memory is not less than 128K
// This code has been tested in STM32F103RDT6, VET6
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
???
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

#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include <stdio.h>
#include "PLC_Dialogue.h"
#include "PLC_IO.h"
#include "PLC_CONF.H"

extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u16 addr);
extern void PLC_BIT_OFF(u16 addr);

const u8 PLC_ROM[110] @ (ROM_ADDR) = {
// The starting address of FLASH is PLC information **************************
// The first 0X02 indicates that the PLC is //16K steps, password area and difference area.
MAXSTEP / 1000,0x00, // 00 0x0000 -- SIZE IN STEP 16000
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

0x0F,0x00, // 92 0x005C -- PROGRAM START  ЅбКшЦёБо
0xFF,0xFF, // 94
0xFF,0xFF, // 96
0xFF,0xFF, // 98
0xFF,0xFF};
  
const u16 special_d[130] = {
0x00C8,  // D8000 - WDT 200mc
0x627A,  // D8001 - (26210/0x6266) FX1N / (25210/0x627A) FX2N ver 2.10 / (16310/0x3FB6) FX3U ver 3.10
MAXSTEP / 1000,  // D8002 - 16000 Steps
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
0,//0x3FB6,  // D8101 - (16310/0x3FB6) FX3U ver 3.10
MAXSTEP / 1000, // D8102 - 16000 Steps
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
const char HEX[]={
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,
0,10,11,12,13,14,15,0,0,0,0,0,0,};

extern u8  PLC_NEW_PROG;

static u16 prog_write_buffer[1024];

static u16 rx_count, tx_count;
char tx_data[144];
char rx_data[144];
static u32 addr;

static u16 prog_address, data_address;

static u8 UART_Sended = 1;
static u8 data_size, bctrl0, bctrl1;
u8  UART_SEND_READY;
u8  PLC_UPDATE_PROG = 0;


void PLC_DATA_INIT(void) {
u32 temp;

  for ( temp = 0; temp < 130; temp++ ) {
    PLC_16BIT(Addr_D8000 + temp * 2) = special_d[temp];
  }

  PLC_8BIT(Addr_M8) = 0x09;	// Turn the M8000 M8003 ON
  bctrl0 = 255;
  bctrl1 = 255;
}

void write_block(u32 number) {
u32 temp, temp1;

  if ( number < (MAXSTEP / 1000 + 1) ) {
    FLASH_Unlock();
    temp1 = number << 11;
    FLASH_ErasePage(ROM_ADDR + temp1);
    for ( temp = 0; temp < 1024; temp++ )
      FLASH_ProgramHalfWord(ROM_ADDR + temp1 + temp * 2, prog_write_buffer[temp]);
    FLASH_Lock();
  }
}

void backup_block(u32 number) {
u32 temp, temp1;

  temp1 = number << 11;
  if ( number < (MAXSTEP / 1000 + 1) ) {
    for ( temp = 0; temp < 1024; temp++ ) {
      prog_write_buffer[temp] = PLC_ROM_16BIT(temp1 + temp * 2);
    }
  }
}

/***********************************************
  єЇКэ№¦ДЬЈє¶ПµзКэѕЭ±ё·Э
***********************************************/
void PLC_DATA_KEEP(void) {  
u32 temp = 0, temp_addr; 

  for ( temp_addr = Addr_D0000 + 1000; temp_addr < Addr_D0000 + 1900; temp_addr +=2 ) // ±ё·Э 400 D µҐФЄµЅ D500
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  for ( temp_addr = Addr_CVal + 200; temp_addr < Addr_CVal + 300; temp_addr +=2 ) // ±ё·Э 50 T  µҐФЄµЅ T100
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  for ( temp_addr = Addr_TVal + 200; temp_addr < Addr_TVal + 300; temp_addr +=2 ) // ±ё·Э 50 D Unit from C100
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  for ( temp_addr = 0x0040; temp_addr < 0x0080; temp_addr +=2 )      // ±ё·Э 512-M1023
    prog_write_buffer[temp] = PLC_16BIT(temp_addr), temp++;

  write_block(10);
}

/***********************************************
  єЇКэ№¦ДЬЈєЙПµзКэѕЭёґО»
***********************************************/
void PLC_DATA_RECOVER(void) { 
u32 temp = 0, temp_addr;

  backup_block(10);

  for ( temp_addr = Addr_D0000 + 1000; temp_addr < Addr_D0000 + 1900; temp_addr +=2 )
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;

  for ( temp_addr = Addr_CVal + 200; temp_addr < Addr_CVal + 300; temp_addr +=2 )
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;

  for ( temp_addr = Addr_TVal + 200; temp_addr < Addr_TVal + 300; temp_addr +=2 )
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;

  for ( temp_addr = 0x0040; temp_addr < 0x0080; temp_addr +=2 )
    PLC_16BIT(temp_addr) = prog_write_buffer[temp], temp++;
}

/*******************************************************************************
єЇКэГыіЖ?PLC_Comm_Byte
єЇКэ№¦ДЬЈєНЁРЕЧЦЅЪµШЦ·ЦШ¶ЁТеј°ЦґРР
ИлїЪІОКэЈєCmdГьБо          
іцїЪІОКэЈєУіЙдµДКµјКµШЦ·(16BITµШЦ·)
********************************************************************************/
/*
u16 PLC_Comm_Byte(u16 comm_add) {     
  if      ( comm_add >= 0x4000 && comm_add <= 0x7E7F ) {return comm_add-0x2000;} //D0000-D7999 
  else if ( comm_add >= 0x0280 && comm_add <= 0x02FC ) {return comm_add-0x0280;} //S000-S999 
  else if ( comm_add >= 0x0240 && comm_add <= 0x0256 ) {return comm_add-0x01C0;} //X000-X277  
  else if ( comm_add >= 0x0180 && comm_add <= 0x0196 ) {return comm_add-0x00E0;} //Y000-Y267 
  else if ( comm_add >= 0x0200 && comm_add <= 0x021F ) {return comm_add-0x0140;} //T00-T255  OVERґҐµг
  else if ( comm_add >= 0x0500 && comm_add <= 0x051F ) {return comm_add-0x0240;} //T00-T255  Enable ПЯИ¦
  else if ( comm_add >= 0x01E0 && comm_add <= 0x01FF ) {return comm_add-0x0020;} //C00-C255  OVERґҐµг
  else if (                       comm_add <= 0x00BF ) {return comm_add+0x0100;} //M0000-M1535 
  else if ( comm_add >= 0x00C0 && comm_add <= 0x017F ) {return comm_add+0x0440;} //M1536-M3071
  else if ( comm_add >= 0x01C0 && comm_add <= 0x01DF ) {return comm_add+0x0020;} //M8000-M8255 
  else return comm_add;                                               //ОЮР§µШЦ·
}
*/
/*******************************************************************************
єЇКэГыіЖЈєPLC_Com_BIT
єЇКэ№¦ДЬЈєНЁРЕО»µШЦ·ЦШ¶ЁТеј°ЦґРР
ИлїЪІОКэЈєCmdГьБо          
іцїЪІОКэЈєУіЙдµДКµјКµШЦ·(BITµШЦ·)
********************************************************************************/
/*
u16 PLC_Com_BIT(u16 addr) {
  if      ( ( addr <= 0x05FF ) )                       return (addr+0x0800); //M0000-M1535
  else if ( ( addr >= 0x0600 ) && ( addr <= 0x0BFF ) ) return (addr+0x2200); //M1536-M3071
  else if ( ( addr >= 0x0C00 ) && ( addr <= 0x0CB7 ) ) return (addr-0x0700); //Y00-Y267
  else if ( ( addr >= 0x1200 ) && ( addr <= 0x12BF ) ) return (addr-0x0E00); //X00-X267  
  else if ( ( addr >= 0x1400 ) && ( addr <= 0x17E7 ) ) return (addr-0X1400); //S00-S999  
  else if ( ( addr >= 0x2800 ) && ( addr <= 0x28FF ) ) return (addr-0X1200); //T00-T255 Enable ПЯИ¦
  else if ( ( addr >= 0x1000 ) && ( addr <= 0x10FF ) ) return (addr-0X0A00); //T00-T255 OVER   ґҐµг
  else if ( ( addr >= 0x0E00 ) && ( addr <= 0x0EFF ) ) return (addr+0X0100); //M8000-M8255
  else if ( ( addr >= 0x0F00 ) && ( addr <= 0x0FFF ) ) return (addr-0X0100); //C00-C255 OVERґҐµг
  else return addr;
}
*/

static u16 find_data(u32 addr,u32 find_data) {
u8 find_ok;

  find_ok = 5;
  addr -= 0x8000;
  do {
    if ( ( PLC_ROM_16BIT(addr) == find_data ) )
      find_ok = 0;
    else
      addr += 2;
    if ( addr > 0x7FFF )
      find_ok = 1;
  } while ( find_ok > 3 );
  addr += 0X8000;
  return addr;
}

void find_data_address(void) {
u32 temp_sum,data_H,data_L;                                    

  data_L = tx_data[5];
  data_H = tx_data[6];
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
  tx_data[7] = ASCII[temp_sum >> 4];
  tx_data[8] = ASCII[temp_sum & 0x0F]; 
  tx_count = 9;
}

/*
void mov_flash(u16 addr,u8 mov_addr) {
u16 start_addr,end_addr,backup_addr,temp,temp1,temp2,mov_byte,addr_mov; 
static u8 offset;

  offset = mov_addr;
  end_addr = find_data(addr + 0x8000, 0x000f) + mov_addr - 0x8000; 
  start_addr = end_addr;
  addr_mov = addr;

  if ( addr > 0x5B ) { 
    addr_mov -= 0X5C;     
    end_addr -= 0x5C;
    addr_mov /= 2;       
    end_addr /= 2;
    addr_mov /= 8;         
    end_addr /= 8;
    offset /= 2;
    mov_byte = offset/8;
    offset %= 8;
    while ( !( end_addr == addr_mov ) ) {
      temp = step_status[end_addr] * 0x100 + step_status[end_addr - 1];   
      temp <<= offset;   
      step_status[end_addr + mov_byte] = temp / 0x100;      
      end_addr--;               
    }
    temp = step_status[end_addr] * 0x100 + step_status[end_addr - 1];
    temp <<= offset;   
    step_status[end_addr + mov_byte] = temp / 0x100;      
  }
  end_addr = start_addr;   
  temp = start_addr;
  do {
    if ( ( end_addr / 0x800 ) == ( addr / 0x800 ) )
      start_addr = addr % 0x800;         
    else
      start_addr = 0;             
    if ( ( temp / 0x800 ) == ( end_addr / 0x800 ) )
      temp1 = end_addr % 0x800 + 1;
    else
      temp1 = 2048;
    backup_block(end_addr / 0x800);
    for ( temp2 = start_addr; temp2 < temp1 + 1; temp2++) {
      backup_addr = ( end_addr / 0x800 ) * 0x800 + temp2 - mov_addr;
//      prog_write_buffer[temp2] = p_x[backup_addr];
    }
    write_block(end_addr / 0x800);
    end_addr -= (temp1 - start_addr);
  } while ( end_addr > addr + mov_addr );
}
*/
// ----------- !!!!!!!!!!!!!!!!!!!!!!!!!!!! ------------------
// Write 8 bit in 16 bit registr
/*
void online_write_data(void) { 
u16 temp;
u8 temp1,temp2;

  temp1 = tx_data[5];      
  temp2 = tx_data[6];     
  temp2 -= temp1;
  if ( temp2 > 0 ) { 
    mov_flash(prog_address - 0x8000, temp2);      
  }
  PLC_NEW_PROG = 0;                       
  bctrl0 = 100;
  bctrl1 = 100;
  prog_address = (tx_data[3] * 0x100 + tx_data[4]) - 0x8000;
  data_size = tx_data[6];
  for ( temp = 0; temp < data_size; temp++ ) {
    bctrl0 = (prog_address + temp) / 0x800;  
    if ( bctrl0 == bctrl1 ) {
      prog_write_buffer[(prog_address + temp) - bctrl0 * 0x800] = tx_data[7 + temp];  
    } else {
      write_block(bctrl1);   
      backup_block(bctrl0);  
      bctrl1 = bctrl0;
      prog_write_buffer[(prog_address + temp) - bctrl0 * 0x800] = tx_data[7 + temp];
    }
  } 
  write_block(bctrl0);   
  tx_data[1] = 0x06;
  tx_count = 1;
}
*/

void UARTSend(void) {
u32 temp;
u8 temp_sum;
u32 data;

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

void Process_switch(void) {                                                                                                                      
u32 temp;
//u8 temp_sum;
u8 data;

  for ( temp = 4; temp < (rx_count - 2); temp++ ) {
    tx_data[temp / 2]  = HEX[rx_data[temp]] * 0x10 + HEX[rx_data[temp + 1]];
    temp++;
  }

  tx_data[0] = 0x06;
  tx_count = 1;

  prog_address = (tx_data[2] << 8) + tx_data[3];
  data_size = tx_data[4];

  switch (rx_data[3]) { 
    case 0x30: // PC_READ_byte();
      addr = RAM_ADDR + prog_address;
      UARTSend();
/*
      tx_data[1] = 0x02;
      temp_sum = 0;
      for ( temp = 0; temp < data_size; temp++ ) { 
        data = PLC_8BIT(temp + prog_address);
        tx_data[temp * 2 + 2] = ASCII[data / 0x10];
        tx_data[temp * 2 + 3] = ASCII[data % 0x10];
        temp_sum += tx_data[temp * 2 + 2] + tx_data[temp * 2 + 3];
      }
      tx_data[temp * 2 + 2] = 0x03;
      temp_sum += 0x03;
      tx_data[temp * 2 + 3] = ASCII[temp_sum / 0x10];
      tx_data[temp * 2 + 4] = ASCII[temp_sum % 0x10]; 
      tx_count = temp * 2 + 4;
*/
      break;

    case 0x31: // PC_WRITE_byte();
      for ( temp = 0; temp < data_size; temp++ ) {
        PLC_8BIT(temp + prog_address) = tx_data[5 + temp];
      }
      break;

    case 0x34: //find_end();                        
      if ( rx_count == 14 ) {
/*
u8 temp_sum,data_H,data_L;                                    

  data_L = tx_data[4];             //РиТЄІйХТКэѕЭµДДЪИЭµНО»  
  data_H = tx_data[5];            //РиТЄІйХТКэѕЭµДДЪИЭёЯО»
  data_address = find_data(prog_address, data_H * 0X100 + data_L);  //¶БFLASH
  tx_data[1] = 0x02;              //±ЁОДїЄКј
  temp_sum = 0;
  tx_data[2] = 0x31;
  temp_sum += tx_data[2];
  data_H = data_address / 0x100;
  data_L = data_address % 0x100;
  tx_data[3] = ASCII[data_H / 0X10];
  tx_data[4] = ASCII[data_H % 0X10];
  tx_data[5] = ASCII[data_L / 0X10];
  tx_data[6] = ASCII[data_L % 0X10];
  tx_data[7] = 0X03;
  temp_sum += tx_data[3];
  temp_sum += tx_data[4];
  temp_sum += tx_data[5];
  temp_sum += tx_data[6];
  temp_sum += tx_data[7];
  tx_data[8] = ASCII[temp_sum / 0x10];
  tx_data[9] = ASCII[temp_sum % 0x10]; 
  tx_count = 9;
*/
        tx_data[0]=0x06, tx_count=1;  
      } else
        tx_data[0]=0x06, tx_count=1; 
      break;

    case 0x37: // PC_FORCE_ON();         
      PLC_BIT_ON(prog_address);
      break;

    case 0x38: // PC_FORCE_OFF();        
      PLC_BIT_OFF(prog_address);
      break;

    case 0x42: // all_flash_lock();
      write_block(bctrl1);
      bctrl1 = 255;
      bctrl0 = 255;
      break;

    case 0x45: // -- Exx -- PC_OPTION_PROG
      data_size = tx_data[5];
      if ( ( rx_count == 11 ) && ( ( rx_data[4] == 0x37 ) || ( rx_data[4] == 0x38 ) ) ) {
        prog_address = (HEX[rx_data[5]] << 4) + HEX[rx_data[6]] + (HEX[rx_data[7]] << 12) + (HEX[rx_data[8]] << 8);
        if ( rx_data[4] == 0x37 ) {
// -- E7 -- EPC_FORCE_ON
          switch(prog_address) {
            case 0x0E23: break;
            case 0x0E24: PLC_8BIT(Addr_M8) = 0x09; PLC_UPDATE_PROG = 0; break;
            case 0x0E25: PLC_8BIT(Addr_M8) = 0x0A; break;
            default:     PLC_8BIT(Addr_M8) = 0x09; PLC_BIT_ON(prog_address); break;
          }
        } else {
// -- E8 -- EPC_FORCE_OFF
          PLC_BIT_OFF(prog_address);
        }
      } else {
        prog_address = (tx_data[3] << 8) + tx_data[4];
        switch (tx_data[2]) { 
          case 0x00: {
// -- E00 -- PC_READ_Parameter
            u32 temp_bit,temp_addr,mov_bit,temp1;
            u32  monitor,monitor1,monitor2; 

            if ( ( prog_address == 0x1790 ) || ( prog_address == 0x17D0 ) ) {          //ЗлЗу¶БјаїШКэѕЭЗш0X1790 Ул 0X17D0µШЦ·
              if ( prog_address == 0x1790 ) {
                monitor1 = PLC_8BIT(0x1400);                              //¶БіцРиТЄјаїШЧЦКэБї 0X1400/2=0XA00
                for ( temp1 = monitor = 0; monitor < monitor1; monitor++ ) {       //¶БјаїШЧЦКэѕЭ
//                  temp_bit = PLC_Comm_Byte(PLC_16BIT[0XA02 + monitor]) / 2;
                  temp_bit = PLC_16BIT(0x1404 + monitor * 2);
                  PLC_16BIT(0x1790 + temp1 * 2) = PLC_16BIT(temp_bit);  //Ѕ«РиТЄµДКэѕЭґ«µЅ»єґж 0X1790/2=0XBC8
                  temp1++;
                  if ( ( temp_bit >= Addr_CHVal ) && ( temp_bit < Addr_CHVal + 56 * 2 ) ) {           //ЦчТЄКЗC200-C255ТФєуµДµШЦ·КЗ32О»µД 
                    PLC_16BIT(0x1790 + temp1 * 2) = PLC_16BIT(temp_bit + 2);     //Ѕ«РиТЄµДКэѕЭґ«µЅ»єґж
                    temp1++;
                  }                      
                }
                monitor2 = PLC_8BIT(0x1402);                            //¶БіцРиТЄјаїШО»КэБї
                for ( monitor1 = 0; monitor1 < monitor2; monitor1++ ) {         //¶БјаїШО»КэѕЭ
//                  temp_addr = PLC_Com_BIT(PLC_16BIT[0XA02 + monitor + monitor1]);
                  temp_addr = PLC_16BIT(0x1404 + (monitor + monitor1) * 2);
//                  temp_bit = PLC_16BIT[temp_addr / 0x10];
                  temp_bit = PLC_16BIT(temp_addr >> 4);
                  mov_bit = temp_addr & 0x0F;
                  if ( ( temp_bit & (1<<mov_bit) ) == ( 1 << mov_bit) )
//                    PLC_16BIT[0xBC8 + temp1 + monitor1 / 0x10] |= 1<<(monitor1 % 0x10);//ґ®їЪ»єґжКэѕЭ
                    PLC_16BIT(0x1790 + temp1 + (monitor1 >> 4)) |= 1 << (monitor1 & 0x0F);//ґ®їЪ»єґжКэѕЭ
                  else
//                    PLC_16BIT[0xBC8 + temp1 + monitor1 / 0x10] &= ~(1<<(monitor1 % 0x10));          
                    PLC_16BIT(0x1790 + temp1 + (monitor1 >> 4)) &= ~(1 << (monitor1 & 0x0F));
                }
              }
            }

            if ( prog_address > 0x3FFF ) // D0000
              prog_address -= 0x2000;

            addr = RAM_ADDR + prog_address;
            UARTSend();
            break;
          }
          case 0x10:
// -- E10 -- PC_WRITE_Parameter
            if ( prog_address < 0x8000 ) { // 0xE420 - Comments

              if ( prog_address > 0x3FFF )
                prog_address -= 0x2000;

              for ( temp = 0; temp < data_size; temp++)
                PLC_8BIT(prog_address + temp) = tx_data[6 + temp];
            }
            break;

          case 0x01:
// -- E01 -- PC_READ_PROG
            addr = ROM_ADDR + prog_address;
            UARTSend();
            break;

          case 0x11:
// -- E11 -- PC_WRITE_PROG
            PLC_8BIT(Addr_M8) = 0x0A; 
            PLC_UPDATE_PROG = 1; // Prevent the program from running during the download process.
            if ( prog_address > 0x7fff ) {
              PLC_NEW_PROG = 1;
              prog_address -= 0x8000;
              data = 0;
              for ( temp = 0; temp < data_size; temp += 2 ) {
                bctrl0 = (prog_address + temp) >> 11; // / 0x800;
                if ( bctrl0 != bctrl1 ) {
                  write_block(bctrl1);   
                  backup_block(bctrl0);  
                  bctrl1 = bctrl0;
                }
                prog_write_buffer[(prog_address + data) - (bctrl0 << 1)] = tx_data[6 + temp] + ( tx_data[7 + temp] << 8);
                data++;
              }
            } else {
              for ( temp = 0; temp < data_size; temp++ ) {
                PLC_8BIT(temp + prog_address) = tx_data[6 + temp];
              }
            } 
            break;

          case 0x41: find_data_address(); break;

          case 0x61: // all_flash_unlock();
            bctrl1 = 255;
            bctrl0 = 255;
          break;

          case 0x60:
// -- E60 -- Clear flash
            backup_block(0);
            prog_write_buffer[46] = 0x000F;  // 0x5C
            for ( temp = 47; temp < 1024; temp++ )
              prog_write_buffer[temp] = 0xFFFF;
            FLASH_Unlock();
            FLASH_ErasePage(ROM_ADDR);
            write_block(0);
            for ( temp = 1; temp < 10; temp++ )
              FLASH_ErasePage(ROM_ADDR + (temp << 11));
            FLASH_Lock();
            break;

          case 0x62:
// -- E62 -- Clear M0000 - M3071
            for ( temp = Addr_ML; temp < Addr_ML + 3072 / 16; temp += 2 )
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
/*
          case 0x0F:
// -- E0F -- Read comment ???? 
            tx_data[1] = 0x02;
            temp_sum = 0;
            prog_address -= 0x8000;
            for ( temp = 0; temp < data_size; temp++ ) { 
              data = PLC_ROM_8BIT(temp + prog_address);
              tx_data[temp * 2 + 2] = ASCII[data / 0x10];
              tx_data[temp * 2 + 3] = ASCII[data % 0x10];
              temp_sum += tx_data[temp * 2 + 2] + tx_data[temp * 2 + 3];
            }
            tx_data[temp * 2 + 2] = 0x03;
            temp_sum += 0x03;
            tx_data[temp * 2 + 3] = ASCII[temp_sum / 0x10];
            tx_data[temp * 2 + 4] = ASCII[temp_sum % 0x10]; 
            tx_count = temp * 2 + 4;
            break;

          case 0x1F:
// -- E1F -- Write comment ???? 
            tx_data[1] = 0x06;
            tx_count = 1;
            break;
*/
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
  rx_count = 0;
  UART_SEND_READY = 1;
}                 

void usart(u16 DEFAULT_BAUD) {
/*
  USART_InitTypeDef USART_InitStructure;
  USART_InitStructure.USART_BaudRate            = DEFAULT_BAUD;
  USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits            = USART_StopBits_1;
  USART_InitStructure.USART_Parity              = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
  USART_Init(USART1,&USART_InitStructure);
*/
  USART1->BRR = SystemCoreClock/DEFAULT_BAUD;
  USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
//  USART1->CR3 |= USART_CR3_DMAT;         //§в§С§Щ§в§Ц§к§Ъ§д§о §б§Ц§в§Ц§Х§С§й§е USART1 §й§Ц§в§Ц§Щ DMA
}  

//---------------------ґ®їЪ№¦ДЬЕдЦГ---------------------
void DMA1_Channel4_IRQHandler(void) {
//  if (DMA1->ISR & DMA_ISR_TCIF4) {    //§б§в§а§У§Ц§в§Ъ§д§о §ж§Э§С§Ф §а§Ь§а§Я§й§С§Я§Ъ§с §а§Т§Ю§Ц§Я§С
    DMA1->IFCR |= DMA1_FLAG_TC4;
    DMA1_Channel4->CCR &= 0xFFFE;
    UART_Sended = 1;
/*  }
  if (DMA1->ISR & DMA_ISR_HTIF4) {    //§б§в§а§У§Ц§в§Ъ§д§о §ж§Э§С§Ф §б§Ц§в§Ц§Х§С§й§Ъ §б§а§Э§а§У§Ъ§Я§н §Т§е§ж§Ц§в§С
    DMA1->IFCR |= DMA_ISR_HTIF4;      //§а§й§Ъ§г§д§Ъ§д§о §ж§Э§С§Ф §б§Ц§в§Ц§Х§С§й§Ъ §б§а§Э§а§У§Ъ§Я§н §Т§е§ж§Ц§в§С
  }
  if (DMA1->ISR & DMA_ISR_TEIF4) {    //§б§в§а§У§Ц§в§Ъ§д§о §ж§Э§С§Ф §а§к§Ъ§Т§Ь§Ъ §а§Т§Ю§Ц§Я§С
    DMA1->IFCR |= DMA_ISR_TEIF4;      //§а§й§Ъ§г§д§Ъ§д§о §ж§Э§С§Ф §а§к§Ъ§Т§Ь§Ъ §а§Т§Ю§Ц§Я§С
  }*/
}

void TX_Process(void) {
  if ( UART_Sended ) {
    UART_SEND_READY = UART_Sended = 0;
    DMA1_Channel4->CNDTR = tx_count;
    DMA1_Channel4->CCR |= 0x0001;
  }
}

void RX_Process(void) {   
static u32 sum, f = 1;
u32 temp;

  rx_data[0] = 0x7F & USART1->DR;

  if ( rx_data[0] == 0X05 ) {
    goto send_ok;

//    rx_count = 0;
//    tx_data[1] = 0x06;
//    tx_count = 1;
//    TX_Process();
  } else if ( rx_data[0] == 0X02 ) {
    rx_count = 0x01;
  } else if ( rx_count == 0 ) {
    if ( f == 1 ) {
      usart(19200);
//      USART1->BRR  = 0x0EA6; //SystemCoreClock/19200;
//      USART1->CR1  = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
      f = 0;

      goto send_ok;
//      rx_count = 0;
//      tx_data[1] = 0x06;
//      tx_count = 1;
//      TX_Process();
    } else if ( f == 0 ) {
      usart(9600);
//      USART1->BRR  = 0x1D4C; //SystemCoreClock/9600;
//      USART1->CR1  = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
      f = 1;
send_ok:
      rx_count = 0;
      tx_data[0] = 0x06;
      tx_count = 1;
      TX_Process();
    }    
  }

  if ( rx_count > 0 ) {
    rx_count++; 
    rx_data[rx_count] = rx_data[0];
    if ( rx_count > 144 ) {
      tx_count = 0;
      rx_count = 0;
    }
    if ( ( rx_count > 3 ) && ( rx_data[rx_count - 2] == 0x03 ) ) {
      sum = 0;
      for ( temp = 3; temp < (rx_count - 1); temp++ )
        sum += rx_data[temp];

      temp = (HEX[rx_data[rx_count - 1]] << 4) + HEX[rx_data[rx_count]];
      if ( ( sum & 0xFF ) == temp ) {
        Process_switch(); 
//        TX_Process();
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

  if ( (USART1->SR & USART_SR_RXNE) != 0 ) {
    USART1->SR &= ~USART_SR_RXNE;
    RX_Process();
  }
  if ( (USART1->SR & USART_SR_ORE) != 0 ) {
    USART1->SR &= ~USART_SR_ORE;
    USART1->DR;
  }
  if ( (USART1->SR & USART_SR_TXE) != 0 ) {
    USART1->SR &= ~USART_SR_TXE;
    USART1->CR1 &= ~USART_CR1_TXEIE;
  }
  if ( (USART1->SR & USART_SR_TC) != 0 ) {
    USART1->SR &= ~USART_SR_TC;
  }
}
