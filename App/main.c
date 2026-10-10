#include "main.h"
#include "PLC_CONF.H"
#include "PLC_Dialogue.h"

extern u8 UART_SEND_READY;
extern char tx_data[144];

extern void X_filter(void);
extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

u16 PLC_RUN_TIME;

int main(void) { 

  InitALL();

  RUN_LED_ON;

  PLC_IO_Refresh();
  PLC_IO_config();                           // Initialize PLC I/O. 
  RTC_Init();                                // Initialize the real-time clock.  

  PLC_DATA_INIT();
  PLC_DATA_RECOVER();

  RUN_LED_OFF;

  RTC_Get();
  while(1) {
    ERR_LED_OFF;  

    PLC_ProInstructParse();

    if ( UART_SEND_READY )
      TX_Process();

  }
}  

void InitALL(void) {

  DMA_InitTypeDef         DMA_InitStructure;
  GPIO_InitTypeDef        GPIO_InitStructure;

  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;       

//=======================================================================================================
//  RCC Configuration
//=======================================================================================================
  RCC->APB1ENR |= RCC_APB1Periph_TIM2
               |  RCC_APB1Periph_I2C1
               |  RCC_APB1Periph_SPI2
  ;
  RCC->APB2ENR |= RCC_APB2Periph_USART1
               |  RCC_APB2Periph_SPI1
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

  USART1->BRR = 0xEA6;                     // 19200 baud at 72 MHz
  USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;
  USART1->CR3 |= USART_CR3_DMAT;         // Enable USART1 transmission through DMA.

  NVIC_EnableIRQ (DMA1_Channel4_IRQn);
  NVIC_EnableIRQ (USART1_IRQn);

  GPIOA->CRH &= ~GPIO_CRH_CNF9;   // Clear CNF bit 9
  GPIOA->CRH |= GPIO_CRH_CNF9_1;  // Set CNF bit 9 to 10 - AFIO Push-Pull
  GPIOA->CRH |= GPIO_CRH_MODE9_0; // Set MODE bit 9 to Mode 01 = 10MHz 




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

  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;   //RS485_TXENA
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

}

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

  }

}

