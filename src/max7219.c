// max7219.c

void max7219_init(const uint8_t br) {

    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

?

    // SCK, MOSI

    GPIOB->CRH |= GPIO_CRH_MODE13;

    GPIOB->CRH &= ~GPIO_CRH_CNF13_0;

    GPIOB->CRH |=  GPIO_CRH_CNF13_1;

?

    GPIOB->CRH |= GPIO_CRH_MODE15;

    GPIOB->CRH &= ~GPIO_CRH_CNF15_0;

    GPIOB->CRH |=  GPIO_CRH_CNF15_1;

?

    // CS

    GPIOB->CRL |= GPIO_CRL_MODE5;

    GPIOB->CRL &= ~GPIO_CRL_CNF5;

?

    CS_DIACTIVATE();

?

    SPI2->CR1 |= SPI_CR1_MSTR;      // master mode

    SPI2->CR1 |= SPI_CR1_BIDIMODE;  // 1 line

    SPI2->CR1 |= SPI_CR1_BIDIOE;    // MOSI

    SPI2->CR1 &= ~SPI_CR1_BR;       // spi_sck = SystemCoreClock / 16 = 8 MHz

    SPI2->CR1 |= SPI_CR1_BR_1;

    SPI2->CR1 &= ~SPI_CR1_LSBFIRST; // MSB

    SPI2->CR1 |= SPI_CR1_DFF;       // 16 bit format

    SPI2->CR1 |= SPI_CR1_SSI;       // software CS

    SPI2->CR1 &= ~SPI_CR1_CPHA;

    SPI2->CR1 &= ~SPI_CR1_CPOL;

?

    SPI1->I2SCFGR &= ~SPI_I2SCFGR_I2SMOD;

?

    SPI2->CR1 |= SPI_CR1_SPE;

?

    // set mode

    max7219_send_to_all(REG_SHUTDOWN,    0x01); // restart

    max7219_send_to_all(REG_DECODE_MODE, 0x00); // use normal mode

    max7219_send_to_all(REG_SCAN_LIMIT,  0x07); // use all lines

?

    max7219_set_brightness(br);

    max7219_clear();

}


void max7219_set_brightness(const uint8_t br) {

    max7219_send_to_all(REG_INTENSITY, br > 15 ? 15 : br);

}


void max7219_clear(void) {

    for (uint32_t i = 0; i < 8; i++) {

        max7219_send_to_all(REG_DIGIT_0 + i, 0x00);

    }

}


#define CS_ACTIVATE()           (GPIOB->ODR &= ~GPIO_ODR_ODR5)

#define CS_DIACTIVATE()         (GPIOB->ODR |= GPIO_ODR_ODR5)


__attribute__((always_inline)) static inline void send_data(uint8_t reg, uint8_t data) {

    SPI2->DR = (uint16_t)( (reg << 8) | data );

    while ( (SPI2->SR & SPI_SR_BSY) == SPI_SR_BSY);

}


void max7219_send_to_all(const uint8_t reg, const uint8_t data) {

    CS_ACTIVATE();

    for (uint32_t i = 0; i < MAX7219_CHIPS; i++) {

        send_data(reg, data);

    }

    CS_DIACTIVATE();

}

#define EMPTY_DATA              0x00


void max7219_send_to_chip(const uint8_t reg, const uint8_t data, const uint8_t chip) {

    if (chip > MAX7219_CHIPS) {

        return;

    }


    CS_ACTIVATE();

    for (uint32_t i = 0; i < MAX7219_CHIPS; i++) {

        if ( (MAX7219_CHIPS - i - 1) == chip)

            send_data(reg, data);

        else

            send_data(REG_NO_OP, data);

    }

    CS_DIACTIVATE();

}

void max7219_send(const uint8_t reg, const uint8_t data_0, const uint8_t data_1) {

    CS_ACTIVATE();

    send_data(reg, data_0);

    send_data(reg, data_1);

    CS_DIACTIVATE();

}

void max7219_test(void) {

    max7219_send_to_all(REG_DIGIT_0, 0x01);

    max7219_send_to_all(REG_DIGIT_1, 0x02);

    max7219_send_to_all(REG_DIGIT_2, 0x04);

    max7219_send_to_all(REG_DIGIT_3, 0x08);

    max7219_send_to_all(REG_DIGIT_4, 0x0F);

    max7219_send_to_all(REG_DIGIT_5, 0x10);

    max7219_send_to_all(REG_DIGIT_6, 0x12);

    max7219_send_to_all(REG_DIGIT_7, 0x14);

}