#include "lcd1602_i2c.h"

uint8_t backlightState = 1 << BL;

void i2c_SendData(uint8_t data_byte) { 
uint8_t index=0;

  I2C1->CR1 |= I2C_CR1_START;
  while (!(I2C1->SR1 & I2C_SR1_SB)) {}
  (void) I2C1->SR1;
  I2C1->DR = 0x7E;
  while (!(I2C1->SR1 & I2C_SR1_ADDR)) {}
  (void) I2C1->SR1;
  (void) I2C1->SR2;
  I2C1->DR = data_byte;
  while (!(I2C1->SR1 & I2C_SR1_TXE)) {}
//  for ( index = 0; index <= count - 1; index++) {
//    I2C1->DR=*data_byte++ ;
//    while (!(I2C1->SR1 & I2C_SR1_TXE)) {}
//  }	
  while (!(I2C1->SR1 & I2C_SR1_BTF)) {}
  I2C1->CR1 |= I2C_CR1_STOP;
}

void lcd_Goto(uint8_t row, uint8_t col) {
#ifdef LCD_2004
  switch (row){
    case 1:
      lcd_Command(0x80 + col);
      break;
    case 2:
      lcd_Command(0x80 + col + 0x40);
      break;
    case 3:
      lcd_Command(0x80 + col + 0x14);
      break;
    case 4:
      lcd_Command(0x80 + col + 0x54);
      break;
  }
#else
  switch (row){
    case 1:
      lcd_Command(0x80 + col);
      break;
    case 2:
      lcd_Command(0x80 + col + 0x40);
      break;
  }
#endif

}

void lcd_PrintC(const uint8_t *str) {
uint8_t i;
  while (i = *str++){
    lcd_Data(i);
  }
}

void lcd_Init(void) {
  backlightState = 0;
  lcd_Command(0x33);
  lcd_pause;
  lcd_Command(0x32); // установка режима: 4 линии
  lcd_Command(0x28); // 2 строки и 5*8 точек
  lcd_Command(0x08); // выключить отображение
  lcd_Command(0x01); // очистка экрана
  lcd_pause;
  lcd_Command(0x06); // направление сдвига курсора
  backlightState = 1 << BL;
  lcd_Command(0x0C); // включить отображение
}

void lcd_Send(uint8_t data) {
  i2c_SendData(data);
}

void lcd_Command(uint32_t com) {
uint8_t data;

  data = ( com & 0xF0 ) | (1 << EN) | backlightState;
  lcd_Send(data);
  lcd_pause;

  data &= ~(1 << EN);
  lcd_Send(data);
  lcd_pause;

  data = (( com & 0x0F ) << 4) | (1 << EN) | backlightState;
  lcd_Send(data);
  lcd_pause;

  data &= ~(1 << EN);
  lcd_Send(data);
  lcd_pause;
}

void lcd_Backlight(uint32_t state) {
  backlightState = (state & 0x01) << BL;
  lcd_Send(backlightState);
}

void lcd_Data(uint32_t com) {
  uint32_t data;

  data = ( com & 0xF0 ) | (1 << EN) | (1 << RS) | backlightState;
  lcd_Send(data);
  lcd_pause;

  data &= ~(1 << EN);
  lcd_Send(data);
  lcd_pause;

  data = (( com & 0x0F ) << 4) | (1 << EN) | (1 << RS) | backlightState;
  lcd_Send(data);
  lcd_pause;

  data &= ~(1 << EN);
  lcd_Send(data);
  lcd_pause;
}
