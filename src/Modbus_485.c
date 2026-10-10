/************************************************* *******/
// CPU needs: STM32F103--RAM memory is not less than 64K Flash memory is not less than 128K
// This code has been tested in STM32F103RDT6, VET6
// Edit date: 20150909
// editor by Xiao Xiaoying
// Online shop: shop182385147.taobao.com
/************************************************* *******/


#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include <stdio.h>
#include "PLC_Dialogue.h"
#include "PLC_IO.h"
#include "PLC_CONF.H"

extern u32 UART3Pause;
extern u32 UART3Timer;

#define MB_Address     0x01   //Slave address test

#define ReadY_Register             0x01 
#define ReadX_Register             0x02 
#define Read_Storage_Register      0x03 
#define Read_Input_Register        0x04
#define WriteForceY_statu          0x05  //Forced single coil
#define WriteForceY_status         0X0F  //Forced multi-coil
#define Write_Storage_Register     0x06  //Preset a register
#define Write_Storage_Registers    0x10  //Preset multiple registers

#define BUFFER_LEN             100

volatile bool receive_flag;
u8  receive_flag1 = 0;
u8 Rx_header;
unsigned char TxBuffer3[100];
unsigned char RxBuffer3[100];
unsigned int crc;
unsigned char TxCounter3 = 0;
unsigned char RxCounter3 = 0;
unsigned char Send3_Size;
unsigned char ST_Scan3 = 0, ST_Scan3V = 0;
unsigned char CMUT, u485;

// u8 RxBuffer3[201] =
// {
// 	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39
// };

/* CRC high byte value table const_ variable value that cannot be modified when executed, code memory space*/
const unsigned char  auchCRCHi[10 *25+6] =
{
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01,
	0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
	0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
	0xC1, 0x81, 0x40
};
/* CRC low byte value table*/
const unsigned char  auchCRCLo[10 *25+6] =
{
	0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05,
	0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A,
	0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B,
	0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14,
	0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3, 0x11,
	0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36,
	0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF,
	0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28,
	0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D,
	0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 0x22,
	0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63,
	0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C,
	0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69,
	0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE,
	0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5, 0x77,
	0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50,
	0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55,
	0x95, 0x94, 0x54, 0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A,
	0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B,
	0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C, 0x44,
	0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41,
	0x81, 0x80, 0x40
};

unsigned int crc16(unsigned char *puchMsg, unsigned int usDataLen) {
unsigned char uchCRCHi = 0xFF; /* High CRC byte initialization */
unsigned char uchCRCLo = 0xFF; /* Low CRC byte initialization */
unsigned long uIndex; /* Index in the CRC loop */

  while (usDataLen--) {
 /* Transfer message buffer */
    uIndex = uchCRCHi ^  *puchMsg++; /* јЖЛгCRC */
    uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex];
    uchCRCLo = auchCRCLo[uIndex];
  }
  return (uchCRCHi << 8 | uchCRCLo);
}

#define CRC16_MODBUS_INIT 0xFFFF
#define CRC16_MODBUS_POLY 0xA001
inline u16 crc16_modbus_upd(u8 data, u16 crc) {
    crc ^= data;
    for (u8 b=8; b; b--) {
        u8 flag_xor = crc & 1;
        crc >>= 1;
        if (flag_xor) crc ^= CRC16_MODBUS_POLY;
    }
    return crc;
}
u16 crc16_modbus_buf(u8 *buf, u16 len, u16 crc) {
    while (len--)
        crc = crc16_modbus_upd(*buf++, crc);
    return(crc);
}
//---------------------Serial port function configuration---------------------
void USART3_Configuration(void) {

  D8130 = 0;   //Default is slave
  D8121 = 1;   //The default slave address is 1
}

void modbus_back() {  //485 MODBUS  RTU 20160502
unsigned char P2, i, j;
unsigned char svd[32];
unsigned int k; //kp;	
	
  switch(RxBuffer3[1]) {
    case ReadY_Register:   //01 Read coil status YY
    svd[0] = RxBuffer3[0]; //ADDR
    svd[1] = RxBuffer3[1]; //FUN
    i = ( RxBuffer3[4] * 256 + RxBuffer3[5] ) / 8;
    svd[2] = i;
    k = ( RxBuffer3[2] * 256 + RxBuffer3[3] ) / 8;
    for ( j = 0; j < i; j++ )
      svd[3 + j] = PLC_8BIT(AddrB_Y + k + j);
			
    crc = crc16(svd, i + 3);
    svd[3 + i] = crc / 256;
    svd[4 + i] = crc;
    TxCounter3 = 0;
    Send3_Size = 0;
    for (j = 0; j <= i + 3; j++)
      TxBuffer3[Send3_Size++] = svd[j];
//    USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
    USART3->CR1 |= USART_CR1_TXEIE;         //прерывание по окончанию передачи
    for (i = 0; i <= 10; i++)
      RxBuffer3[i] = 0;
    break;

    case ReadX_Register:   //02 Read the digital input state XX group
    svd[0] = RxBuffer3[0]; //ADDR
    svd[1] = RxBuffer3[1]; //FUN
    i = ( RxBuffer3[4] * 256 + RxBuffer3[5] ) / 8;
    svd[2] = i;
    k = ( RxBuffer3[2] * 256 + RxBuffer3[3] ) / 8;
    for ( j = 0; j < i; j++ )
      svd[3 + j] = PLC_8BIT(AddrB_X + k + j);
			
    crc = crc16(svd, i + 3);
    svd[3 + i] = crc / 256;
    svd[4 + i] = crc;

    TxCounter3 = 0;
    Send3_Size = 0;
    for (j = 0; j <= i + 3; j++)
      TxBuffer3[Send3_Size++] = svd[j];
    USART3->CR1 |= USART_CR1_TXEIE;         //прерывание по окончанию передачи
//    USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
    for (i = 0; i <= 10; i++)
      RxBuffer3[i] = 0;
    break;

    case Read_Storage_Register :    //03  Read holding register
    case Read_Input_Register :      //04  Read input register
    svd[0] = RxBuffer3[0]; //ADDR
    svd[1] = RxBuffer3[1]; //FUN
    svd[2] = RxBuffer3[5] * 2; //BYTES
    k = RxBuffer3[2] * 256 + RxBuffer3[3];

    for (i = 0; i < RxBuffer3[5]; i++) {
      if ((k + i) <= 5999) {
        svd[3 + i * 2] = PLC_8BIT(Addr_D0000 + i);
        svd[4 + i * 2] = PLC_8BIT(Addr_D0000 + i);
      }

      if ((k + i) >= 8000) {
        svd[3 + i * 2] = PLC_8BIT(k + i - 2000);
        svd[4 + i * 2] = PLC_8BIT(k + i - 2000);
      }
    }

    crc = crc16(svd, svd[2] + 3);
    svd[svd[2] + 3] = crc / 256;
    svd[svd[2] + 4] = crc;
    TxCounter3 = 0;
    Send3_Size = 0;
    for (i = 0; i <= (svd[2] + 4); i++)
      TxBuffer3[Send3_Size++] = svd[i];
    USART3->CR1 |= USART_CR1_TXEIE;         //прерывание по окончанию передачи
//    USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
//RE1;u485=5;
    for (i = 0; i <= 10; i++)
      RxBuffer3[i] = 0;
    break ;

    case Write_Storage_Register :   //06 Preset a register
    k = RxBuffer3[2] * 256 + RxBuffer3[3];
    if (k <= 5999) {
      PLC_8BIT(Addr_D0000 + k) = RxBuffer3[4] + RxBuffer3[5] * 256;
    }
    if (k >= 8000) {
    //k = k - 2000;
      PLC_8BIT(Addr_D0000 + k) = RxBuffer3[4] + RxBuffer3[5] * 256;
    }

    TxCounter3 = 0;
    Send3_Size = 0;
    for (i = 0; i <= 7; i++)
      TxBuffer3[Send3_Size++] = RxBuffer3[i];
    //The single register is set successfully and the data is returned as it is.
    USART3->CR1 |= USART_CR1_TXEIE;         //прерывание по окончанию передачи
//    USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
    // RE1;u485=5;
    for (i = 0; i <= 10; i++)
      RxBuffer3[i] = 0;
    break;

    case WriteForceY_statu:	       //05 Forced single coil
    k = RxBuffer3[2] * 256 + RxBuffer3[3];
    PLC_16BIT(AddrB_Y + k) = RxBuffer3[5] + RxBuffer3[4] * 256;
    TxCounter3 = 0;
    Send3_Size = 0;
    for (i = 0; i <= 7; i++)
      TxBuffer3[Send3_Size++] = RxBuffer3[i];
    //The single register is set successfully and the data is returned as it is.
    USART3->CR1 |= USART_CR1_TXEIE;         //прерывание по окончанию передачи
//    USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
    // RE1;u485=5;
    for (i = 0; i <= 10; i++)
      RxBuffer3[i] = 0;
    break;

    case WriteForceY_status:	       //0F Forced multi-coil
    break;

    case Write_Storage_Registers :    //0X10  Set multiple registers
    k = RxBuffer3[2] * 256 + RxBuffer3[3];
    if (k <= 5999) {
      PLC_16BIT(Addr_D0000 + k) = RxBuffer3[7] + RxBuffer3[8] * 256;
    }
    if (k >= 8000) {
      PLC_16BIT(Addr_D0000 + k) = RxBuffer3[7] + RxBuffer3[8] * 256;
    }

    TxCounter3 = 0;
    Send3_Size = 0;
    for (i = 0; i < 6; i++)
      TxBuffer3[Send3_Size++] = RxBuffer3[i];
    crc = crc16(RxBuffer3, 6);
    P2 = crc / 256;
    TxBuffer3[Send3_Size++] = P2;
    P2 = crc;
    TxBuffer3[Send3_Size++] = P2;
    USART3->CR1 |= USART_CR1_TXEIE;         //прерывание по окончанию передачи
//    USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
    //RE1;u485=5;
    for (i = 0; i <= 10; i++)
      RxBuffer3[i] = 0;

    break ;
    default :break ;
  }
}

void mosbus_485_USART3(void) {
u8 j;

  UART3Pause = 0;
  if ( D8130 == 0 ) { //Slave test mask
    if ( RxBuffer3[0] == D8121 ) { //D8121 is the modbus station address	
      if ( RxCounter3 > 8 ) { //11BIT
        crc = crc16(RxBuffer3, 9);
        if ( crc  == ( RxBuffer3[10] + RxBuffer3[9] * 256 ) ) //P=crc;
          modbus_back();
            // USART_SendData(USART3, 0X76);
      } else { //8 bit
        crc = crc16(RxBuffer3, 6); 
        if ( crc  == ( RxBuffer3[7] + RxBuffer3[6] * 256 ) ) //P=crc;
          modbus_back();
            // USART_SendData(USART3, 0X56);
      } 
    }
  } else
    if (RxBuffer3[0] == D8122) { //Substation return register
      crc = crc16(RxBuffer3, 5);
      if ( crc  == ( RxBuffer3[6] + RxBuffer3[5] * 256 ) ) { //P=crc;
        PLC_8BIT(Addr_D0000 + 78) = RxBuffer3[4];
        PLC_8BIT(Addr_D0000 + 79) = RxBuffer3[3];
        for (j = 0; j <= 10; j++)
          RxBuffer3[j] = 0;
        RxCounter3 = 0;
      }
    }
  RxCounter3 = 0;
  receive_flag = TRUE;
}

/*******************************************************************************
 * Function Name  : USART3_IRQHandler
 * Description    : This function handles USART3 global interrupt request.
 * Input          : None
 * Output         : None
 * Return         : None
 *******************************************************************************/
void USART3_IRQHandler(void) {

  if ( (USART3->SR & USART_SR_RXNE) != 0 ) {
    USART3->SR &= ~USART_SR_RXNE;

    UART3Timer = 4; // 4 millisec = 3.5 symbol 9600 b/sec
    RxBuffer3[RxCounter3] = USART3->DR;
    if ( RxCounter3 < BUFFER_LEN )
      RxCounter3++;
  }
	 
  if ( (USART3->SR & USART_SR_ORE) != 0 ) {
    USART3->SR &= ~USART_SR_ORE;
    USART3->DR;
  }

  if ( (USART3->SR & USART_SR_TXE) != 0 ) {
    USART3->SR &= ~USART_SR_TXE;
    USART3->DR = TxBuffer3[TxCounter3++];
    if ( TxCounter3 == Send3_Size) {
      USART3->CR1 &= ~USART_CR1_TXEIE;
    }
  }
  if ( (USART3->SR & USART_SR_TC) != 0 ) {
    USART3->SR &= ~USART_SR_TC;
  }
}
