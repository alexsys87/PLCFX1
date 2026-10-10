
#ifndef _MAIN_H_
#define _MAIN_H_
#include "stm32f10x.h"
#include <plc_io.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

void InitALL(void);

extern void PLC_ProInstructParse(void);
extern void RTC_Init(void);
extern void RTC_Get(void);
//extern void TIM3_PWM_Init(void);
//extern void pulse_Init(void);
//extern void RST_C(void);
//extern void RTC_Get_Init(void);
//extern void PLC_Timer(void);
//extern void A_B_Init(void);
//extern void NCIC_Confinguration(FunctionalState NewState);
//extern void ADC_init(void);
//extern void I2C_MyInit(void);
//extern void backup_data(void);
extern void PLC_IO_Refresh(void);
extern void PLC_IO_config(void);
extern void TX_Process(void);          //发送串口数据
extern void PLC_DATA_KEEP(void); 	
extern void PLC_DATA_RECOVER(void);        //断电数据恢复  
//extern void usart(u16 i);
//extern void USART1_Configuration(void);
//extern void USART3_Configuration(void);
extern void PLC_DATA_INIT(void);           //D8000~D8126
extern void mosbus_485_USART3(void);

//extern void IIC_Init(void);
//extern void I2C_LowLevel_Init(I2C_TypeDef* I2Cx);

//extern void ssd1306_Init(void);

extern void lcd_Init(void); // ????????????? ???
extern void lcd_Goto(uint8_t row, uint8_t col); // ??????? ?? ??????/???????
extern void lcd_PrintC(const uint8_t *str); // ??????? ??????

#endif
