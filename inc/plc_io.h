/****************************************************************************
PLC-related special registers
Dedicated auxiliary relays     Description
M8126                          Global flag
M8127                          Communication request handshake signal
M8128                          Error flag
M8129                          Communication request switching

Dedicated data registers       Description
D8000 = 200;                   Scan time
D8001 = 0X5EF6;                Model/version FX2N(C)
D8101 = 0X5EF6;                Model/version FX2N(C)
D8002 = 8;                     Memory capacity
D8102 = 8;                     Memory capacity
D8003 = 0x0010;                Memory type, register type
D8006                         CPU battery voltage
D8010 = 10;                    Current scan time
D8011 = 20;                    Minimum scan time (0.1 ms)
D8012 = 140;                   Maximum scan time (0.1 ms)

D8120 = 0X4096                 Communication format
D8121                         Slave number (up to 16)
D8127                         Start address of exchanged data
D8128                         Amount of exchanged data
D8129                         Network communication timeout confirmation value
D8000                         Watchdog

Communication format details (D8120)
----------------------------------------------------------------------
Bit        | Meaning       | Description
-----------+---------------+------------------------------------------
b0         | Data length   | 0: 7 bits; 1: 8 bits
-----------+---------------+------------------------------------------
b2b1       | Parity        | 00: none; 01: odd; 11: even
-----------+---------------+------------------------------------------
b3         | Stop bits     | 0: 1 bit; 1: 2 bits
-----------+---------------+------------------------------------------
           |               | 0001: 300      0111: 4800
b7b6b5b4   | Baud rate     | 0100: 600      1000: 9600
           |               | 0101: 1200     1001: 19200
           |               | 0110: 2400
-----------+---------------+------------------------------------------
b8         |               | 0: unused; for no-protocol communication only
-----------+---------------+------------------------------------------
b9         |               | 0: unused; same as above
-----------+---------------+------------------------------------------
b12b11b10  | Interface     | 000: RS485 (RS422) interface
           |               | 010: RS232C interface
-----------+---------------+------------------------------------------
b13        | Checksum      | 0: no checksum; 1: append checksum automatically
-----------+---------------+------------------------------------------
b14        | Protocol      | 0: no-protocol communication; 1: dedicated protocol
-----------+---------------+------------------------------------------
b15        | Format        | 0: format 1; 1: format 4
----------------------------------------------------------------------

Example: D8120 = 0X4096 sets the communication baud rate to 19200

*********************************************************************************/
#ifndef _PLC_IO_H 
#define _PLC_IO_H 

#include "platform.h"              //0X4221018C

#ifdef __cplusplus
extern "C"  {
#endif
#ifdef __cplusplus
}
#endif

#define BIT_ADDR(addr, bitnum)         *((volatile unsigned long  *)((0x42000000)+(addr<<5)+(bitnum<<2)))                   //
//I/O port address mapping                                                                
#define GPIOA_ODR_Addr    0x01080C  
#define GPIOB_ODR_Addr    0x010C0C 
#define GPIOC_ODR_Addr    0x01100C 
#define GPIOD_ODR_Addr    0x01140C 
//I/O mode definitions
#define GPIOA_IDR_Addr    0x010808 
#define GPIOB_IDR_Addr    0x010C08 
#define GPIOC_IDR_Addr    0x011008 
#define GPIOD_IDR_Addr    0x011408 
//I/O operations: only for a single I/O pin!

//Ensure that n is less than 16!
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  //Output 
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //Input 

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //Output 
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //Input 

#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  //Output 
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  //Input 

#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  //Output 
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  //Input 

#define PLC_RUN    PBin(8)
#define PVD        PAin(0)

#define RUN_LED_ON    PCout(13)=0
#define RUN_LED_OFF   PCout(13)=1 

#define ERR_LED_ON    PBout(12)=0
#define ERR_LED_OFF   PBout(12)=1 

//X input definitions
#define X00 PAin(1)	
#define X01 PAin(2)
#define X02 PAin(3)
#define X03 PAin(6)
#define X04 PAin(7)		 
#define X05 PCin(4)
#define X06 PCin(5)
#define X07 PBin(0)

// Users may extend these definitions as needed

//Y output definitions
#define Y00 PCout(9)  
#define Y01 PAout(8)  
#define Y02 PAout(11)
#define Y03 PAout(12)
#define Y04 PAout(15)   
#define Y05 PCout(10) 
#define Y06 PCout(11) 
#define Y07 PCout(12)

// Users may extend these definitions as needed

#endif
