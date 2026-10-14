/**
 * @file main.c
 * @brief Firmware entry point: board bring-up and the PLC super-loop (BluePill, STM32F103C8T6).
 *
 * Boot sequence: clocks and peripherals -> GPIO -> RTC -> retained data -> runtime in STOP.
 * The PLC then runs forever as a cooperative loop; a 1 ms timer interrupt only maintains
 * time bases (scan clock, timers, input debounce):
 *
 *   loop:  sample inputs -> run one scan -> start a pending serial reply -> drive outputs
 *          -> update the RUN/ERR LEDs -> refresh the calendar registers once per second
 *
 * RUN/STOP is a software state controlled by the programming protocol (see
 * plc_serial_protocol.c); the PLC boots in STOP, including after a failed download.
 */
#include "board_clock.h"
#include "plc_io.h"
#include "plc_memory.h"
#include "plc_memory_map.h"
#include "plc_rtc.h"
#include "plc_runtime.h"
#include "plc_serial_protocol.h"
#include "plc_special_devices.h"

/** Milliseconds since reset, incremented by the 1 ms timer interrupt (saturates at 65535). */
volatile u16 PLC_RUN_TIME;

static volatile uint32_t uptime_ms;      /* free-running millisecond counter (wraps after 49 days) */

static void init_peripherals(void);

/** Firmware entry point: initialize the hardware and the PLC, then run the scan loop forever. */
int main(void)
{
  plc_memory_init();
  init_peripherals();

  RUN_LED_ON;                            /* lit while booting */

  PLC_IO_config();                       /* outputs are low before they become outputs */
  PLC_IO_Refresh();
  RTC_Init();                            /* initialize the real-time clock */

  PLC_DATA_INIT();                       /* special registers and boot-in-STOP relay state */
  PLC_Runtime_Init();                    /* boot in STOP, including after a bad download */

  RUN_LED_OFF;

  RTC_Get();
  TIM2->CR1 = TIM_CR1_CEN;               /* start ticks only after runtime initialization */

  while (1) {
    static uint32_t last_rtc;

    PLC_IO_SampleInputs();               /* freeze the inputs for one complete scan */
    PLC_ProInstructParse();              /* execute one scan of the PLC program */

    if (UART_SEND_READY)
      TX_Process();                      /* start the DMA transmission of a prepared reply */

    PLC_IO_Refresh();                    /* drive Y0..Y7 (forced low outside RUN) */

    /* RUN LED: PLC running without a fault. ERR LED: a fault is latched. */
    if (PLC_Runtime_RunRequested() && (PLC_8BIT(Addr_M8) & 3U) == 1U && PLC_Runtime_LastError() == 0U) {
      RUN_LED_ON;
    } else {
      RUN_LED_OFF;
    }
    if (PLC_Runtime_LastError() != 0U) {
      ERR_LED_ON;
    } else {
      ERR_LED_OFF;
    }

    if ((uint32_t)(uptime_ms - last_rtc) >= 1000U) {
      last_rtc = uptime_ms;
      RTC_Get();                         /* refresh D8013..D8019 once per second */
    }
  }
}

/** Configure clocks, USART1 with DMA transmit, the status LEDs and the 1 ms PLC tick timer. */
static void init_peripherals(void)
{
  (void)board_clock_init();

  RCC->AHBENR |= RCC_AHBENR_DMA1EN;
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN |
                  RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  (void)RCC->APB2ENR;
  (void)RCC->APB1ENR;
  AFIO->MAPR = AFIO_MAPR_SWJ_CFG_JTAGDISABLE; /* keep SWD, release the JTAG pins */

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
  /* main enables the timer after PLC state and I/O have been initialized. */
}

/** 1 ms tick: time bases for the runtime and the input debounce filter. */
void TIM2_IRQHandler(void)
{
  TIM2->SR = 0U;                         /* acknowledge the update interrupt */
  uptime_ms++;
  if (PLC_RUN_TIME != UINT16_MAX) PLC_RUN_TIME++;
  PLC_Runtime_Tick1ms();
  X_filter();
}
