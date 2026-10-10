/********************************************************/
/********************************************************
********************************************************/

#include "main.h"
#include "PLC_CONF.h"
#include "PLC_Dialogue.h"

#include "lcd1602_i2c.h"

//#define SSD1306_USE_SPI

//#include "ssd1306.h"
//#include "ssd1306_fonts.h"

//#include "n3310.h"

//#include <stdio.h>

extern u8 UART_SEND_READY;
extern char tx_data[144];

extern void X_filter(void);
extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

extern bool receive_flag;
extern u8 Rx_header;

u16 PLC_RUN_TIME;

//#define USE_ADC
//#define USE_I2C
//#define USE_SPI1
//#define USE_SPI2
//#define USE_USART2
//#define USE_USART3
//#define USE_MODBUS

#ifdef USE_ADC
#define N  64  //Number of samples per channel
#define M  8   //Five channels in total
// Note: The ADC is a 12-bit analog-to-digital converter, only the lower 12 bits of ADCConvertedValue are valid.
__IO uint16_t ADCConvertedValue[N][M];
#endif

//__IO uint8_t LCD[16*2];

//u8  power_down;
//u8  run_flag;
u16 Timer1;
u16 UART2Timer;
u8  UART2Pause;
u16 UART3Timer;
u8  UART3Pause;

#ifdef __GNUC__
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

void delay(uint32_t t) {
	uint32_t i = 0;
	for (; i < t; i++);
}


//char str[32];

int main(void) { 
    
//  power_down = 10;

  InitALL();

  RUN_LED_ON;

  PLC_IO_Refresh();
  PLC_IO_config();                           // PLCКдИлКдіціхКј»Ї 
  RTC_Init();                                // RTCК±ЦУЕдЦГ  
/*
  LcdInit();
  LcdGotoXYFont(1,1);
  LcdStr(FONT_1X, (unsigned char *)"Hello World!");
  LcdUpdate();
  
  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_SetCursor(2, 0);
  ssd1306_WriteString("Font 16x26", Font_16x26, White);
  ssd1306_SetCursor(2, 26);
  ssd1306_WriteString("Font 11x18", Font_11x18, White);
  ssd1306_SetCursor(2, 26+18);
  ssd1306_WriteString("Font 7x10", Font_7x10, White);
  ssd1306_UpdateScreen();

//  lcd_Init();
//  lcd_Goto(1, 0);
//  lcd_PrintC("12345678901234567890123456789012");
//  lcd_Goto(1, 0);
//  lcd_PrintC("FX2N Ver 3.8");
*/
  PLC_DATA_INIT();
  PLC_DATA_RECOVER();

//  printf("FX2N Started\r\n");
  RUN_LED_OFF;

  RTC_Get();
  while(1) {
    ERR_LED_OFF;  

    PLC_ProInstructParse();

//    lcd_Goto(2, 0);
//    sprintf(str, "D8010 = %u", D8010);
//    lcd_PrintC(str);

    if ( UART_SEND_READY )
      TX_Process();

#ifdef USE_MODBUS
    if ( UART3Pause )
      mosbus_485_USART3();
#endif                                
/*    
    if ( !PVD ) {    //MY PCB== !PVD
      if ( Timer1 == 0 )
        PLC_DATA_RECOVER();  
      if ( Timer1 <= 60000 )
        Timer1++;
    } else {
      if ( Timer1 >= 100 ) {
        PLC_16BIT(Addr_Y) = PLC_16BIT(Addr_Y + 2) = 0;
        PLC_DATA_KEEP(); 
//        NCIC_Confinguration(DISABLE);
        Timer1 = 0;
      }
    }   
*/
  }
}  

void InitALL(void) {
  ADC_InitTypeDef         ADC_InitStructure;
  DMA_InitTypeDef         DMA_InitStructure;
  GPIO_InitTypeDef        GPIO_InitStructure;
  I2C_InitTypeDef         I2C_InitStructure;
//  USART_InitTypeDef       USART_InitStructure;
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;       
  SPI_InitTypeDef         SPI_InitStructure;
//  NVIC_InitTypeDef        NVIC_InitStructure;

//=======================================================================================================
//  RCC Configuration
//=======================================================================================================
  RCC->APB1ENR |= RCC_APB1Periph_TIM2
               |  RCC_APB1Periph_I2C1
               |  RCC_APB1Periph_SPI2
//               |  RCC_APB1Periph_USART3
//               |  RCC_APB1Periph_USART2
  ;
  RCC->APB2ENR |= RCC_APB2Periph_USART1
               |  RCC_APB2Periph_SPI1
//               |  RCC_APB2Periph_ADC1
               |  RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
  RCC->AHBENR  |= RCC_AHBPeriph_DMA1;

// alternative func enable
  AFIO->MAPR = AFIO_MAPR_SWJ_CFG_JTAGDISABLE  // disable JTAG
             | AFIO_MAPR_SPI1_REMAP           // remap SPI1 to port B
  ;

//=======================================================================================================
//  USART1 Configuration
//=======================================================================================================
  DMA_DeInit(DMA1_Channel4);
  DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&USART1->DR);
  DMA_InitStructure.DMA_MemoryBaseAddr     = (u32)tx_data;
  DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;
  DMA_InitStructure.DMA_BufferSize         = 144;
  DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_MemoryDataSize     = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
  DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
  DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
  DMA_Init(DMA1_Channel4, &DMA_InitStructure);
  DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);

//  DMA1_Channel4->CPAR  = (u32)(&USART1->DR);
//  DMA1_Channel4->CMAR  = (u32)tx_data;
//  DMA1_Channel4->CNDTR = 144;
//  DMA1_Channel4->CCR   =  0;                 //предочистка регистра конфигурации
//  DMA1_Channel4->CCR  |=  DMA_CCR4_PINC;     //инкремент указателя
//  DMA1_Channel4->CCR  |=  DMA_CCR4_DIR;      //направление: чтение из памяти

  USART1->BRR = 0xEA6;                     // Bodrate for 19200 on 72Mhz
  USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
//  USART1->CR1 |= USART_CR1_TCIE;         //прерывание по окончанию передачи
  USART1->CR3 |= USART_CR3_DMAT;         //разрешить передачу USART1 через DMA

  NVIC_EnableIRQ (DMA1_Channel4_IRQn);
  NVIC_EnableIRQ (USART1_IRQn);
/*  
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
*/
  GPIOA->CRH &= ~GPIO_CRH_CNF9;   // Clear CNF bit 9
  GPIOA->CRH |= GPIO_CRH_CNF9_1;  // Set CNF bit 9 to 10 - AFIO Push-Pull
  GPIOA->CRH |= GPIO_CRH_MODE9_0; // Set MODE bit 9 to Mode 01 = 10MHz 
//=======================================================================================================
//  USART2 Configuration
//=======================================================================================================
#ifdef USE_USART2
  USART2->BRR = 0xEA6;                     // Bodrate for 19200 on 72Mhz
  USART2->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
//  USART2->CR1 |= USART_CR1_TCIE;         //прерывание по окончанию передачи

  NVIC_EnableIRQ (USART2_IRQn);

  GPIOA->CRL &= ~GPIO_CRH_CNF2;   // Clear CNF bit 9
  GPIOA->CRL |= GPIO_CRH_CNF2_1;  // Set CNF bit 9 to 10 - AFIO Push-Pull
  GPIOA->CRL |= GPIO_CRH_MODE2_0; // Set MODE bit 9 to Mode 01 = 10MHz 

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
#endif
//=======================================================================================================
//  USART3 Configuration
//=======================================================================================================
#ifdef USE_USART2
  USART3->BRR = 0xEA6;                     // Bodrate for 19200 on 72Mhz
  USART3->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
//  USART3->CR1 |= USART_CR1_TCIE;         //прерывание по окончанию передачи

  NVIC_EnableIRQ (USART3_IRQn);

  GPIOB->CRH &= ~GPIO_CRH_CNF10;   // Clear CNF bit 9
  GPIOB->CRH |= GPIO_CRH_CNF10_1;  // Set CNF bit 9 to 10 - AFIO Push-Pull
  GPIOB->CRH |= GPIO_CRH_MODE10_0; // Set MODE bit 9 to Mode 01 = 10MHz 

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
#endif
//=======================================================================================================
//  ADC1 Configuration
//=======================================================================================================
#ifdef USE_ADC
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0|GPIO_Pin_1;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  DMA_DeInit(DMA1_Channel1);
  DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1->DR;
  DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)&ADCConvertedValue;
  DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
  DMA_InitStructure.DMA_BufferSize         = N*M;
  DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
  DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
  DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;
  DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
  DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
  DMA_Init(DMA1_Channel1, &DMA_InitStructure);         
//  DMA_Cmd(DMA1_Channel1, ENABLE);

  ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
  ADC_InitStructure.ADC_ScanConvMode       = ENABLE;
  ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
  ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
  ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
  ADC_InitStructure.ADC_NbrOfChannel       = M;
  ADC_Init(ADC1, &ADC_InitStructure);

// ADC1 regular channel10 configuration ADC Channel group, 10th channel Sampling sequence 1, conversion time
  ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 1, ADC_SampleTime_239Cycles5);
  ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 2, ADC_SampleTime_239Cycles5);
  ADC_RegularChannelConfig(ADC1, ADC_Channel_12, 4, ADC_SampleTime_239Cycles5);
  ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 5, ADC_SampleTime_239Cycles5);
  ADC_RegularChannelConfig(ADC1, ADC_Channel_14, 6, ADC_SampleTime_239Cycles5);
  ADC_RegularChannelConfig(ADC1, ADC_Channel_15, 7, ADC_SampleTime_239Cycles5);
  ADC_RegularChannelConfig(ADC1, ADC_Channel_16, 8, ADC_SampleTime_239Cycles5);
  ADC_RegularChannelConfig(ADC1, ADC_Channel_17, 9, ADC_SampleTime_239Cycles5);
	
//  ADC_DMACmd(ADC1, ENABLE);	  
//  ADC_Cmd(ADC1, ENABLE);  
  ADC_ResetCalibration(ADC1);	  
  while(ADC_GetResetCalibrationStatus(ADC1));
  ADC_StartCalibration(ADC1);		
  while(ADC_GetCalibrationStatus(ADC1));	  
  ADC_SoftwareStartConvCmd(ADC1, ENABLE);	
#endif
//=======================================================================================================
//  I2С Configuration
//=======================================================================================================
/*
  DMA_DeInit(DMA1_Channel6);
  DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&I2C1->DR);
  DMA_InitStructure.DMA_MemoryBaseAddr     = (u32)i2c_tx_data;
  DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;
  DMA_InitStructure.DMA_BufferSize         = 144;
  DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_MemoryDataSize     = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
  DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
  DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
  DMA_Init(DMA1_Channel6, &DMA_InitStructure);
//  DMA_ITConfig(DMA1_Channel6, DMA_IT_TC, ENABLE);
*/
#ifdef USE_I2C
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_7;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  RCC->APB1RSTR |= RCC_APB1Periph_I2C1;
  RCC->APB1RSTR &= ~RCC_APB1Periph_I2C1;

  I2C_InitStructure.I2C_ClockSpeed          = 100000;
  I2C_InitStructure.I2C_Mode                = I2C_Mode_I2C;
  I2C_InitStructure.I2C_DutyCycle           = I2C_DutyCycle_2;
  I2C_InitStructure.I2C_OwnAddress1         = 0x7E;
  I2C_InitStructure.I2C_Ack                 = I2C_Ack_Enable;
  I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
  I2C_Init(I2C1, &I2C_InitStructure);
//  I2C_ITConfig(I2C1, I2C_IT_EVT, ENABLE);
//  I2C_DMACmd(I2C1, ENABLE);
  I2C_Cmd(I2C1, ENABLE);
//  NVIC_EnableIRQ (I2C1_EV_IRQn);
#endif
//=======================================================================================================
//  TIMER2 Configuration
//=======================================================================================================
  TIM_TimeBaseStructure.TIM_Period        = 36000;
  TIM_TimeBaseStructure.TIM_Prescaler     = 1;
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
  TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
  TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
  TIM_Cmd(TIM2, ENABLE);
//=======================================================================================================
//  TIMER3 Configuration
//=======================================================================================================
/*
  TIM_TimeBaseStructure.TIM_Period        = 36000;
  TIM_TimeBaseStructure.TIM_Prescaler     = 1;
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
  TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
  TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
  TIM_Cmd(TIM3, ENABLE);
*/
//=======================================================================================================
//  SPI Configuration
//=======================================================================================================

#ifdef USE_SPI1
  SPI1->CR1 = SPI_CR1_SPE   // разрешить SPI2 
//   | SPI_CR1_BR_0 //настройка делителя 
   | SPI_CR1_BR_1    //комментируя или раскомментируя эти строки можно изменять значение делителя 
   | SPI_CR1_BR_2
   | SPI_CR1_SSM   // программное управление слэйвом
   | SPI_CR1_SSI   // при программном управлении слэйвом данный бит должен быть 1
   | SPI_CR1_MSTR;  // Режим мастера
  SPI1->CR2 = SPI_CR2_TXEIE | SPI_CR2_RXNEIE;

  // GPIO pins for MOSI, MISO, and SCK
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
//  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_15;   //SPI1 CS
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
/*
// SS                           
   GPIOA->CRH  |=  GPIO_CRH_MODE15;    //A15
   GPIOA->CRH  &= ~GPIO_CRH_CNF15;     //
   GPIOA->BSRR  =  GPIO_BSRR_BS15;     //

//SCK
   GPIOB->CRL  |=  GPIO_CRL_MODE3;    //B3
   GPIOB->CRL  &= ~GPIO_CRL_CNF3;     //
   GPIOB->CRL  |=  GPIO_CRL_CNF3_1;   //

// MISO
   GPIOB->CRL  &= ~GPIO_CRL_MODE4;    //B4
   GPIOB->CRL  &= ~GPIO_CRL_CNF4;     //
   GPIOB->CRL  |=  GPIO_CRL_CNF4_1;   //
   GPIOB->BSRR  =  GPIO_BSRR_BS4;     //

//MOSI
   GPIOB->CRL  |=  GPIO_CRL_MODE5;    //B5
   GPIOB->CRL  &= ~GPIO_CRL_CNF5;     //
   GPIOB->CRL  |=  GPIO_CRL_CNF5_1;   //
*/
#endif

#ifdef USE_SPI2
  SPI2->CR1 = SPI_CR1_SPE   // разрешить SPI2 
//   | SPI_CR1_BR_0 //настройка делителя 
   | SPI_CR1_BR_1    //комментируя или раскомментируя эти строки можно изменять значение делителя 
   | SPI_CR1_BR_2
//   | SPI_CR1_CPHA
   | SPI_CR1_SSM   // программное управление слэйвом
   | SPI_CR1_SSI   // при программном управлении слэйвом данный бит должен быть 1
   | SPI_CR1_MSTR;  // Режим мастера
//  SPI2->CR2 = SPI_CR2_TXEIE | SPI_CR2_RXNEIE;

  // GPIO pins for MOSI, MISO, and SCK
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_15;
//  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_14;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
/*
// SS                           
//   GPIOB->CRH  |=  GPIO_CRH_MODE12;    //B12
//   GPIOB->CRH  &= ~GPIO_CRH_CNF12;     //
//   GPIOB->BSRR  =  GPIO_BSRR_BS12;     //

//SCK
   GPIOB->CRH  |=  GPIO_CRH_MODE13;    //B13
   GPIOB->CRH  &= ~GPIO_CRH_CNF13;     //
   GPIOB->CRH  |=  GPIO_CRH_CNF13_1;   //

// MISO
   GPIOB->CRH  &= ~GPIO_CRH_MODE14;    //B14
   GPIOB->CRH  &= ~GPIO_CRH_CNF14;     //
   GPIOB->CRH  |=  GPIO_CRH_CNF14_1;   //
   GPIOB->BSRR  =  GPIO_BSRR_BS14;     //

//MOSI
   GPIOB->CRH  |=  GPIO_CRH_MODE15;    //B15
   GPIOB->CRH  &= ~GPIO_CRH_CNF15;     //
   GPIOB->CRH  |=  GPIO_CRH_CNF15_1;   //
*/
#endif
//=======================================================================================================
//  OTHER IO Configuration
//=======================================================================================================
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;   //SHUTDOWN
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB,&GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;   //RUN/STOP_SWITCH
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB,&GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;  //RUN_LED
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOC,&GPIO_InitStructure);

//  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;  //ERR_LED
//  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
//  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//  GPIO_Init(GPIOB,&GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;   //RS485_TXENA
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
//=======================================================================================================
//  OTHER IO Configuration
//=======================================================================================================
//  NVIC_SetPriorityGrouping(5);

#ifdef USE_ADC
  NVIC_EnableIRQ (DMA1_Channel1_IRQn);
  NVIC_SetPriority (DMA1_Channel1_IRQn, 0xC);
#endif
/*
  NVIC_EnableIRQ (DMA1_Channel4_IRQn);
  NVIC_SetPriority (DMA1_Channel4_IRQn, 0xD);
  NVIC_EnableIRQ (DMA1_Channel6_IRQn);
  NVIC_SetPriority (DMA1_Channel6_IRQn, 0xE);

  NVIC_EnableIRQ (USART1_IRQn);
  NVIC_SetPriority (USART1_IRQn, 0x8);

  NVIC_EnableIRQ (USART2_IRQn);
  NVIC_SetPriority (USART2_IRQn, 0x9);
  NVIC_EnableIRQ (USART3_IRQn);
  NVIC_SetPriority (USART3_IRQn, 0xA);

  NVIC_EnableIRQ (I2C1_EV_IRQn);
  NVIC_SetPriority (I2C1_EV_IRQn, 0x0);
  NVIC_EnableIRQ (I2C1_ER_IRQn);
  NVIC_SetPriority (I2C1_ER_IRQn, 0x1);

  NVIC_EnableIRQ (TIM2_IRQn);
  NVIC_SetPriority (TIM2_IRQn, 0x2);

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2 );

  NVIC_InitStructure.NVIC_IRQChannel                   = DMA1_Channel1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
	
  NVIC_InitStructure.NVIC_IRQChannel                   = DMA1_Channel4_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
	
  NVIC_InitStructure.NVIC_IRQChannel                   = DMA1_Channel6_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
	
  NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; 
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE; 
  NVIC_Init(&NVIC_InitStructure);
  
  NVIC_InitStructure.NVIC_IRQChannel                   = USART2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; 
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE; 
  NVIC_Init(&NVIC_InitStructure);
  
  NVIC_InitStructure.NVIC_IRQChannel                   = USART3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; 
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE; 
  NVIC_Init(&NVIC_InitStructure);
		 								   
  NVIC_InitStructure.NVIC_IRQChannel                   = TIM2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure); 
		 								   
  NVIC_InitStructure.NVIC_IRQChannel                   = I2C1_EV_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure); 
		 								   
  NVIC_InitStructure.NVIC_IRQChannel                   = I2C1_ER_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure); 
	  
  NVIC_InitStructure.NVIC_IRQChannel                   = TIM3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
		 
  NVIC_InitStructure.NVIC_IRQChannel                   = TIM4_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
		 
  NVIC_InitStructure.NVIC_IRQChannel                   = TIM5_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
		 
  NVIC_InitStructure.NVIC_IRQChannel                   = EXTI15_10_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure); 
		 
  NVIC_InitStructure.NVIC_IRQChannel                   = EXTI9_5_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
  NVIC_Init(&NVIC_InitStructure); 
*/
//  NVIC_EnableIRQ(I2C1_EV_IRQn);
//  NVIC_EnableIRQ(I2C1_ER_IRQn);
}

#ifdef USE_ADC
void DMA1_Channel1_IRQHandler(void) {
u32 sum = 0;
u32 count, i;

  if (DMA1->ISR & DMA_ISR_TCIF1) {    //проверить флаг окончания обмена
    DMA1->IFCR |= DMA1_FLAG_TC1;
    DMA1_Channel1->CCR &= 0xFFFE;
    for ( i = 0; i < M; i++ ) { 
      for ( count = 0; count < N; count++ ) {
        sum += ADCConvertedValue[count][i];
      }
      PLC_16BIT(6030 * 2 + i * 2) = sum / N;
      sum = 0;
    }
    DMA1_Channel1->CNDTR = N*M;
    DMA1_Channel1->CCR |= 0x0001;
  }
  if (DMA1->ISR & DMA_ISR_HTIF1) {    //проверить флаг передачи половины буфера
    DMA1->IFCR |= DMA_ISR_HTIF1;        //очистить флаг передачи половины буфера
  }
  if (DMA1->ISR & DMA_ISR_TEIF1) {    //проверить флаг ошибки обмена
    DMA1->IFCR |= DMA_ISR_TEIF1;        //очистить флаг ошибки обмена
  }
}
#endif
/*
void DMA1_Channel6_IRQHandler(void) {
  if (DMA1->ISR & DMA_ISR_TCIF6) {    //проверить флаг окончания обмена
    DMA1->IFCR |= DMA1_FLAG_TC6;
    DMA1_Channel6->CCR &= 0xFFFE;
  }
}
*/
void TimerInc(u32 s, u32 e) { 
u32 timer_count;

  for ( timer_count = s; timer_count < e; timer_count++ ) {
    if ( PLC_BIT_TEST(AddrB_TE * 8 + timer_count) ) {
      if ( PLC_16BIT(Addr_TVal + timer_count * 2) < PLC_16BIT(Addr_TMax + timer_count * 2) ) {
        if ( !PLC_BIT_TEST(AddrB_TO * 8 + timer_count) ) ;
          PLC_16BIT(Addr_TVal + timer_count * 2)++;
      }
    }
  }
}

void TIM2_IRQHandler(void) { //1ms
static u8 state1, state10, state100, state1000;

  TIM2->SR = 0;   
  PLC_RUN_TIME++;
  state1++;

  TimerInc(246, 250); // T_1MS();

  X_filter();

//m8011 10MS m8012 100MS  m8013 1SEC m8014 1minute   

  if ( state1 == 5 ) {
    TimerInc(200, 246); //    T_10MS();
    PLC_BIT_OFF(M8011);
  } else

  if ( state1 == 7 ) {
//    DAC_data();
//    filter();
  } else

  if ( state1 > 9 ) {
    state1 = 0;
    PLC_BIT_ON(M8011);

    state10++;

    if ( state10 == 5 ) {
      TimerInc(0, 200); //     T_100MS();
      PLC_BIT_OFF(M8012);  
    } else

    if ( state10 == 9 ) {
      TimerInc(250, 256); //     T_H100MS();
    } else

    if ( state10 > 9 ) {
      state10 = 0;
      PLC_BIT_ON(M8012);

      state100++;

      if ( state100 == 5 ) {
        PLC_BIT_OFF(M8013);
      } else

      if ( state100 > 9 ) {
        state100 = 0;
        PLC_BIT_ON(M8013);

        state1000++;

        if ( state1000 == 30 )
          PLC_BIT_OFF(M8014);

        if ( state1000 > 59 ) {
          state1000 = 0;      
          PLC_BIT_ON(M8014);
        }
      }
    }

////    receive_flag = FALSE;  //ADD
////    Rx_header = FALSE;
  }
  if ( UART3Timer != 0 ) {
    if ( --UART3Timer == 0 ) {
      UART3Pause = 1;
    }
  }
}

void TIM3_IRQHandler(void) { //1ms
  TIM3->SR = 0;   
}

#ifdef USE_SPI1
void SPI1_IRQHandler(void) {
//uint16_t tmp;
 
  if ( SPI1->SR & SPI_SR_TXE ) { //причина прерывания - окончание передачи байта
     //выполняем какие-то действия      
  }
 
  if ( SPI1->SR & SPI_SR_RXNE ) { //причина прерывания - принят байт данных
//     tmp = SPI1->DR;                //прочитать принятые данные
     //выполняем какие-то действия      
  }
}
#endif

#ifdef USE_SPI2
void SPI2_IRQHandler(void) {
//uint16_t tmp;
 
  if ( SPI2->SR & SPI_SR_TXE ) { //причина прерывания - окончание передачи байта
     //выполняем какие-то действия      
  }
 
  if ( SPI2->SR & SPI_SR_RXNE ) { //причина прерывания - принят байт данных
//     tmp = SPI2->DR;                //прочитать принятые данные
     //выполняем какие-то действия      
  }
}
#endif
/*
PUTCHAR_PROTOTYPE
{
  USART_SendData(USART3, (uint8_t) ch);
  while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
  return ch;
}
*/
