#include "main.h"
#include "board_clock.h"
#include "plc_memory.h"
#include "PLC_CONF.H"
#include "PLC_Dialogue.h"

extern volatile u8 UART_SEND_READY;
extern char tx_data[144];

extern void X_filter(void);
extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

volatile u16 PLC_RUN_TIME;

int main(void) { 

  plc_memory_init();
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
  (void)board_clock_init();

  RCC->AHBENR |= RCC_AHBENR_DMA1EN;
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN |
                  RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  (void)RCC->APB2ENR;
  (void)RCC->APB1ENR;
  AFIO->MAPR = AFIO_MAPR_SWJ_CFG_JTAGDISABLE; /* Keep SWD, release JTAG pins. */

  /* USART1: PA9 alternate push-pull, PA10 floating input, 19200 baud, 8N1. */
  GPIOA->CRH = (GPIOA->CRH & ~((0xFUL << 4) | (0xFUL << 8))) |
               (0xBUL << 4) | (0x4UL << 8);
  USART1->CR1 = 0U;
  USART1->CR2 = 0U;
  USART1->CR3 = USART_CR3_DMAT;
  USART1->BRR = (board_apb2_hz() + 9600U) / 19200U;

  /* DMA1 channel 4: byte-sized, memory increment, memory-to-USART TX. */
  DMA1_Channel4->CCR = 0U;
  DMA1->IFCR = DMA_IFCR_CGIF4;
  DMA1_Channel4->CPAR = (uint32_t)&USART1->DR;
  DMA1_Channel4->CMAR = (uint32_t)tx_data;
  DMA1_Channel4->CNDTR = 0U;
  DMA1_Channel4->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PL_1 |
                       DMA_CCR_TCIE | DMA_CCR_TEIE;
  NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
  NVIC_SetPriority(DMA1_Channel4_IRQn, 3U);
  NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  NVIC_ClearPendingIRQ(USART1_IRQn);
  NVIC_SetPriority(USART1_IRQn, 2U);
  NVIC_EnableIRQ(USART1_IRQn);
  USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE | USART_CR1_RXNEIE;

  /* PB8 is the external RUN input. A disconnected board defaults to STOP. */
  GPIOB->BRR = GPIO_BRR_BR8;
  GPIOB->CRH = (GPIOB->CRH & ~0xFUL) | 0x8UL;

  /* BluePill PC13 active-low LED. PB12 supports an optional error LED. */
  GPIOC->BSRR = GPIO_BSRR_BS13;
  GPIOC->CRH = (GPIOC->CRH & ~(0xFUL << 20)) | (0x6UL << 20);
  GPIOB->BSRR = GPIO_BSRR_BS12;
  GPIOB->CRH = (GPIOB->CRH & ~(0xFUL << 16)) | (0x6UL << 16);

  /* Exact 1 ms PLC tick at 72 MHz, 64 MHz or reset-HSI fallback. */
  TIM2->CR1 = 0U;
  TIM2->PSC = board_apb1_timer_hz() / 1000000U - 1U;
  TIM2->ARR = 999U;
  TIM2->CNT = 0U;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->SR = 0U;
  TIM2->DIER = TIM_DIER_UIE;
  NVIC_ClearPendingIRQ(TIM2_IRQn);
  NVIC_SetPriority(TIM2_IRQn, 4U);
  NVIC_EnableIRQ(TIM2_IRQn);
  TIM2->CR1 = TIM_CR1_CEN;
}

void TimerInc(u32 s, u32 e) { 
u32 timer_count;

  for ( timer_count = s; timer_count < e; timer_count++ ) {
    if ( PLC_BIT_TEST(AddrB_TE * 8 + timer_count) ) {
      if ( PLC_16BIT(Addr_TVal + timer_count * 2) < PLC_16BIT(Addr_TMax + timer_count * 2) ) {
        if ( !PLC_BIT_TEST(AddrB_TO * 8 + timer_count) )
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

