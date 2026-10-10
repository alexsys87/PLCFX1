#ifndef __MAX7219_H__

#define __MAX7219_H__

?

#include "stm32f10x.h"

?

/* REGs */

?

#define MAX7219_CHIPS        2

?

void max7219_init(const uint8_t br);

void max7219_set_brightness(const uint8_t br);

void max7219_clear(void);

?

void max7219_send_to_all(const uint8_t reg, const uint8_t data);

void max7219_send_to_chip(const uint8_t reg, const uint8_t data, const uint8_t chip);

void max7219_send(const uint8_t reg, const uint8_t data_0, const uint8_t data_1);

?

void max7219_test(void);

?

#endif /* __MAX7219_H__ */