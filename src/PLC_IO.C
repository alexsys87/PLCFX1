/********************************************************/
// CPU需要：STM32F103--RAM内存不小于64K  Flash内存不小于128K
// 本代码已在STM32F103RDT6、VET6测试通过
// 编辑日期：20150909
// editor by 小小晟
// 网店：shop182385147.taobao.com
/********************************************************/

#include "PLC_IO.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "absacc.h" 
#include "PLC_CONF.H"
#include "PLC_Dialogue.h"

extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

u16  X_DIY = 10;

//======================================================================================================
// 函数名称:  void PLC_X_config(void)   
// 功能描述： PLC_运行状态IO初始化
// 输　入:   void  
// 输　出:   void   
// 全局变量:  
// 调用模块: 
// 作　者:  小小晟
// 日　期:  2014年6月13日
// 备  注:  
//------------------------------------------------------------------------------------------------------
// 修改人:
// 日　期:
// 备  注: 
//=======================================================================================================
void PLC_EXTI15_10_config(void) { 
/*
  EXTI_InitTypeDef EXTI_InitStructure;
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOA,GPIO_PinSource12|GPIO_PinSource11);
  EXTI_InitStructure.EXTI_Line=EXTI_Line12;
  EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Falling;
  EXTI_InitStructure.EXTI_LineCmd=ENABLE;
  EXTI_Init(&EXTI_InitStructure);
  
  EXTI_InitStructure.EXTI_Line=EXTI_Line11;
  EXTI_Init(&EXTI_InitStructure);  
*/
}

//======================================================================================================
// 函数名称:  void PLC_X_config(void)   
// 功能描述： PLC_运行状态IO初始化
// 输　入:   void  
// 输　出:   void   
// 全局变量:  
// 调用模块: 
// 作　者:  小小晟
// 日　期:  2014年6月13日
// 备  注:  
//------------------------------------------------------------------------------------------------------
// 修改人:
// 日　期:
// 备  注: 
//=======================================================================================================
void PLC_EXTI9_5_config(void) { 
/*
  EXTI_InitTypeDef EXTI_InitStructure;
  
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource9);
  EXTI_InitStructure.EXTI_Line=EXTI_Line9;
  EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Falling;
  EXTI_InitStructure.EXTI_LineCmd=ENABLE;
  EXTI_Init(&EXTI_InitStructure);
  
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource8);
  EXTI_InitStructure.EXTI_Line=EXTI_Line8;
  EXTI_Init(&EXTI_InitStructure);  
  
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource7);
  EXTI_InitStructure.EXTI_Line=EXTI_Line7;
  EXTI_Init(&EXTI_InitStructure);  
  
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource6);
  EXTI_InitStructure.EXTI_Line=EXTI_Line6;
  EXTI_Init(&EXTI_InitStructure);    
*/
}



//======================================================================================================
// 函数名称:  void PLC_X_config(void)   
// 功能描述： PLC_运行状态IO初始化
// 输　入:   void  
// 输　出:   void   
// 全局变量:  
// 调用模块: 
// 作　者:  小小晟
// 日　期:  2014年6月13日
// 备  注:  
//------------------------------------------------------------------------------------------------------
// 修改人:
// 日　期:
// 备  注: 
//=======================================================================================================
void PLC_Mode_config(void) { 
//  GPIO_InitTypeDef GPIO_InitStructure;
  
  //GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);陈凡注释
    //define EEPROM
//   GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10|GPIO_Pin_11;     //EEPROM
//   GPIO_InitStructure.GPIO_Mode =  GPIO_Mode_Out_OD;
//   GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//   GPIO_Init(GPIOB,&GPIO_InitStructure);
  

//  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 ;                 //PLC_485通讯
//  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//  GPIO_Init(GPIOD, &GPIO_InitStructure);
}

//======================================================================================================
// 函数名称:  void PLC_X_config(void)   
// 功能描述： PLC_X_输入初始化
// 输　入:   void  
// 输　出:   void   
// 全局变量:  
// 调用模块: 
// 作　者:  小小晟
// 日　期:  2014年6月13日
// 备  注:  
//------------------------------------------------------------------------------------------------------
// 修改人:
// 日　期:
// 备  注: 
//=======================================================================================================
void PLC_X_config(void) { 
/*  GPIO_InitTypeDef GPIO_InitStructure;

  //GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);陈凡注释  

//define X00-X14
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_6 | GPIO_Pin_7;  
  GPIO_InitStructure.GPIO_Mode =  GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOA,&GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Mode =  GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB,&GPIO_InitStructure);    
  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
  GPIO_InitStructure.GPIO_Mode =  GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOC,&GPIO_InitStructure);    
*/
}

//======================================================================================================
// 函数名称:  PLC_Y_config(void)   
// 功能描述： PLC_Y_输出初始化
// 输　入:   void  
// 输　出:   void   
// 全局变量:  
// 调用模块: 
// 作　者:  小小晟
// 日　期:  2014年6月13日
// 备  注:  
//------------------------------------------------------------------------------------------------------
// 修改人:
// 日　期:
// 备  注: 
//=======================================================================================================
void PLC_Y_config(void) { 
/*  GPIO_InitTypeDef GPIO_InitStructure;

  //GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE); 陈凡注释  

//define Y00-Y15
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 ;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  
//  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_2 ;
//  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
//  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//  GPIO_Init(GPIOD, &GPIO_InitStructure);
*/
}

void X_filter(void) {                //每　1ms  需调用一次，用于X滤波，暂定为20MS
static u16 x_buffer[8];   //暂定刷新16个X 
/*
  if ( X00 ) {
    x_buffer[0] = 0;
    PLC_BIT_OFF(Addr_X * 8 + 0x00);
  } else
    if ( x_buffer[0] < X_DIY )
      x_buffer[0]++;
    else
      PLC_BIT_ON(Addr_X * 8 + 0x00);
*/
//  (X00) ? (x_buffer[0] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x00)) : (x_buffer[0] < X_DIY)  ? (x_buffer[0]++) : (PLC_BIT_ON(Addr_X * 8 + 0x00));
//  (X01) ? (x_buffer[1] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x01)) : (x_buffer[1] < X_DIY)  ? (x_buffer[1]++) : (PLC_BIT_ON(Addr_X * 8 + 0x01));  
//  (X02) ? (x_buffer[2] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x02)) : (x_buffer[2] < X_DIY)  ? (x_buffer[2]++) : (PLC_BIT_ON(Addr_X * 8 + 0x02));  
//  (X03) ? (x_buffer[3] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x03)) : (x_buffer[3] < X_DIY)  ? (x_buffer[3]++) : (PLC_BIT_ON(Addr_X * 8 + 0x03));  
//  (X04) ? (x_buffer[4] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x04)) : (x_buffer[4] < X_DIY)  ? (x_buffer[4]++) : (PLC_BIT_ON(Addr_X * 8 + 0x04));  
//  (X05) ? (x_buffer[5] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x05)) : (x_buffer[5] < X_DIY)  ? (x_buffer[5]++) : (PLC_BIT_ON(Addr_X * 8 + 0x05));  
//  (X06) ? (x_buffer[6] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x06)) : (x_buffer[6] < X_DIY)  ? (x_buffer[6]++) : (PLC_BIT_ON(Addr_X * 8 + 0x06));  
//  (X07) ? (x_buffer[7] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x07)) : (x_buffer[7] < X_DIY)  ? (x_buffer[7]++) : (PLC_BIT_ON(Addr_X * 8 + 0x07));
/*  (X10) ? (x_buffer[8] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x08)) : (x_buffer[8] < D8020)  ? (x_buffer[8]++) : (PLC_BIT_ON(Addr_X * 8 + 0x08));
  (X11) ? (x_buffer[9] = 0, PLC_BIT_OFF(Addr_X * 8 + 0x09)) : (x_buffer[9] < D8020)  ? (x_buffer[9]++) : (PLC_BIT_ON(Addr_X * 8 + 0x09));  
  (X12) ? (x_buffer[10]= 0, PLC_BIT_OFF(Addr_X * 8 + 0x0A)) : (x_buffer[10]< D8020) ? (x_buffer[10]++) : (PLC_BIT_ON(Addr_X * 8 + 0x0A));  
  (X13) ? (x_buffer[11]= 0, PLC_BIT_OFF(Addr_X * 8 + 0x0B)) : (x_buffer[11]< D8020) ? (x_buffer[11]++) : (PLC_BIT_ON(Addr_X * 8 + 0x0B));  
  (X14) ? (x_buffer[12]= 0, PLC_BIT_OFF(Addr_X * 8 + 0x0C)) : (x_buffer[12]< D8020) ? (x_buffer[12]++) : (PLC_BIT_ON(Addr_X * 8 + 0x0C));  
  (X15) ? (x_buffer[13]= 0, PLC_BIT_OFF(Addr_X * 8 + 0x0D)) : (x_buffer[13]< D8020) ? (x_buffer[13]++) : (PLC_BIT_ON(Addr_X * 8 + 0x0D));  
  (X16) ? (x_buffer[14]= 0, PLC_BIT_OFF(Addr_X * 8 + 0x0E)) : (x_buffer[14]< D8020) ? (x_buffer[14]++) : (PLC_BIT_ON(Addr_X * 8 + 0x0E));  
  (X17) ? (x_buffer[15]= 0, PLC_BIT_OFF(Addr_X * 8 + 0x0F)) : (x_buffer[15]< D8020) ? (x_buffer[15]++) : (PLC_BIT_ON(Addr_X * 8 + 0x0F));
*/
  //上电断电检测  
//   if(!PVD) {if(power_down<=7)power_down++;}else{if(power_down>0)power_down--;}
}


//刷新输出，一个地址为32个点
void PLC_IO_Refresh(void) {
//  Y00 = PLC_BIT_TEST(Addr_Y * 8 + 0x00);
//  Y01 = PLC_BIT_TEST(Addr_Y * 8 + 0x01);
//  Y02 = PLC_BIT_TEST(Addr_Y * 8 + 0x02);
//  Y03 = PLC_BIT_TEST(Addr_Y * 8 + 0x03);
//  Y04 = PLC_BIT_TEST(Addr_Y * 8 + 0x04);
//  Y05 = PLC_BIT_TEST(Addr_Y * 8 + 0x05);
//  Y06 = PLC_BIT_TEST(Addr_Y * 8 + 0x06);
//  Y07 = PLC_BIT_TEST(Addr_Y * 8 + 0x07);
//  Y10 = PLC_BIT_TEST(Addr_Y * 8 + 0x08);
//  Y11 = PLC_BIT_TEST(Addr_Y * 8 + 0x09);
//  Y12 = PLC_BIT_TEST(Addr_Y * 8 + 0x0A);
//  Y13 = PLC_BIT_TEST(Addr_Y * 8 + 0x0B);
//  Y14 = PLC_BIT_TEST(Addr_Y * 8 + 0x0C);
//  Y15 = PLC_BIT_TEST(Addr_Y * 8 + 0x0D);
//  Y16 = PLC_BIT_TEST(514);
//  Y17 = PLC_BIT_TEST(515);
}
/*
void SPI_init(void) {

// SPI2 init
  RCC->APB1ENR |= RCC_APB1ENR_SPI2EN; // такты на SPI2

  GPIOB->CRH &= (~(GPIO_CRH_CNF13_0 | GPIO_CRH_CNF15_0)); //настройка выводов для SPI
  GPIOB->CRH |= (GPIO_CRH_MODE13 | GPIO_CRH_MODE15                
   | GPIO_CRH_CNF13_1 | GPIO_CRH_CNF15_1);                // AF Push-Pull out

  SPI2->CR1 |= //SPI_CR1_BR_1    //настройка делителя 
    SPI_CR1_BR_0    //комментируя или раскомментируя эти строки можно изменять значение делителя 
   | SPI_CR1_BR_2
   | SPI_CR1_SSM   // программное управление слэйвом
   | SPI_CR1_SSI   // при программном управлении слэйвом данный бит должен быть 1
   | SPI_CR1_MSTR  // Режим мастера
   | SPI_CR1_SPE;   // разрешить SPI2 

  SPI2->CR2 |= SPI_CR2_RXDMAEN // разрешить передачу принятых данных через DMA
   | SPI_CR2_TXDMAEN; // Разрешить принимать данные для передачи через DMA
}
*/
/*
void spi1_init() {
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);  // тактирование порта
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1,ENABLE);  // тактирование SPI1
 
   GPIO_InitTypeDef gpio;
    GPIO_StructInit(&gpio);
 
    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOA,&gpio);
 
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource5,GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource6,GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource7,GPIO_AF_SPI1);
 
    SPI_I2S_DeInit(SPI1);
    SPI_InitTypeDef spi1;
    SPI_StructInit(&spi1);
 
    spi1.SPI_Mode = SPI_Mode_Master;
    spi1.SPI_DataSize = SPI_DataSize_16b;
    spi1.SPI_NSS = SPI_NSS_Soft;
    SPI_Init(SPI1,&spi1);
    SPI_Cmd(SPI1,ENABLE);
}

void spi_send(uint16_t data) {
    SPI_I2S_SendData(SPI1,data);
    while(SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_TXE) == RESET);  // ждём пока данные уйдут
}
 
uint16_t spi_receve() {
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);  // ждём пока данные появтся
    uint16_t received = SPI_I2S_ReceiveData(SPI2);
    return received;
}
*/
/*
void SPIx_Init()
{
    // Initialization struct
    SPI_InitTypeDef SPI_InitStruct;
    GPIO_InitTypeDef GPIO_InitStruct;
 
    // Step 1: Initialize SPI
    RCC_APB2PeriphClockCmd(SPIx_RCC, ENABLE);
    SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
    SPI_InitStruct.SPI_NSS = SPI_NSS_Soft | SPI_NSSInternalSoft_Set;
    SPI_Init(SPIx, &SPI_InitStruct); 
    SPI_Cmd(SPIx, ENABLE);
 
    // Step 2: Initialize GPIO
    RCC_APB2PeriphClockCmd(SPI_GPIO_RCC, ENABLE);
    // GPIO pins for MOSI, MISO, and SCK
    GPIO_InitStruct.GPIO_Pin = SPI_PIN_MOSI | SPI_PIN_MISO | SPI_PIN_SCK;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_GPIO, &GPIO_InitStruct);
    // GPIO pin for SS
    GPIO_InitStruct.GPIO_Pin = SPI_PIN_SS;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SPI_GPIO, &GPIO_InitStruct);
 
    // Disable SPI slave device
    SPIx_DisableSlave();
}
 
uint8_t SPIx_Transfer(uint8_t data)
{
    // Write data to be transmitted to the SPI data register
    SPIx->DR = data;
    // Wait until transmit complete
    while (!(SPIx->SR & (SPI_I2S_FLAG_TXE)));
    // Wait until receive complete
    while (!(SPIx->SR & (SPI_I2S_FLAG_RXNE)));
    // Wait until SPI is not busy anymore
    while (SPIx->SR & (SPI_I2S_FLAG_BSY));
    // Return received data from SPI data register
    return SPIx->DR;
}
*/
void PLC_IO_config(void) {
//  RCC->APB2ENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;

//  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);
//  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO , ENABLE);

//  Err_Lamp_OFF;
//  PLC_Y_config();
//  PLC_X_config();
//  PLC_Mode_config();
//  PLC_EXTI15_10_config();
//  PLC_EXTI9_5_config();
}
