static uint8_t SSD1306_Command[2];

uint8_t ctrl, len;
uint8_t *data;

//-----------------------------------------------
typedef enum
{
    Polling = 0x00,
    Interrupt = 0x01,
    DMA = 0x02
} I2C_ProgrammingModel;

typedef enum
{
    Error = 0,
    Success = !Error
}Status;

#define I2C_DIRECTION_TX 0
#define I2C_DIRECTION_RX 1

DMA_InitTypeDef  I2CDMA_InitStructure;

__IO uint8_t Tx_Idx1=0, Rx_Idx1=0;
__IO uint8_t Tx_Idx2=0, Rx_Idx2=0;

__IO uint32_t NumbOfBytes1;
__IO uint32_t NumbOfBytes2;
uint8_t Buffer_Rx1[255];
uint8_t Buffer_Tx1[255];

__IO uint32_t I2CDirection ;
uint8_t Address;
//-----------------------------------------------

void I2C1_EV_IRQHandler(void) {

    __IO uint32_t SR1Register =0;
    __IO uint32_t SR2Register =0;

    /* Read the I2C1 SR1 and SR2 status registers */
    SR1Register = I2C1->SR1;
    SR2Register = I2C1->SR2;

    /* If I2C1 is slave (MSL flag = 0) */
    if ((SR2Register &0x0001) != 0x0001) {
        /* If ADDR = 1: EV1 */
        if ((SR1Register & 0x0002) == 0x0002) {
            /* Clear SR1Register and SR2Register variables to prepare for next IT */
            SR1Register = 0;
            SR2Register = 0;
            /* Initialize the transmit/receive counters for next transmission/reception
            using Interrupt  */
            Tx_Idx1 = 0;
            Rx_Idx1 = 0;
        }
        /* If TXE = 1: EV3 */
        if ((SR1Register & 0x0080) == 0x0080) {
            /* Write data in data register */
            I2C1->DR = Buffer_Tx1[Tx_Idx1++];
            SR1Register = 0;
            SR2Register = 0;
        }
        /* If RXNE = 1: EV2 */
        if ((SR1Register & 0x0040) == 0x0040) {
            /* Read data from data register */
            Buffer_Rx1[Rx_Idx1++] = I2C1->DR;
            SR1Register = 0;
            SR2Register = 0;

        }
        /* If STOPF =1: EV4 (Slave has detected a STOP condition on the bus */
        if (( SR1Register & 0x0010) == 0x0010) {
//            I2C1->CR1 |= CR1_PE_Set;
            SR1Register = 0;
            SR2Register = 0;
        }
    } /* End slave mode */

    /* If SB = 1, I2C1 master sent a START on the bus: EV5) */
    if ((SR1Register &0x0001) == 0x0001) {
        /* Send the slave address for transmssion or for reception (according to the configured value
            in the write master write routine */
        I2C1->DR = Address;
        SR1Register = 0;
        SR2Register = 0;
    }
    /* If I2C1 is Master (MSL flag = 1) */
    if ((SR2Register &0x0001) == 0x0001) {
        /* If ADDR = 1, EV6 */
        if ((SR1Register &0x0002) == 0x0002) {
            /* Write the first data in case the Master is Transmitter */
            if (I2CDirection == I2C_DIRECTION_TX) {
                /* Initialize the Transmit counter */
                Tx_Idx1 = 0;
                /* Write the first data in the data register */
                I2C1->DR = Buffer_Tx1[Tx_Idx1++];
                /* Decrement the number of bytes to be written */
                NumbOfBytes1--;
                /* If no further data to be sent, disable the I2C BUF IT
                in order to not have a TxE  interrupt */
                if (NumbOfBytes1 == 0) {
                    I2C1->CR2 &= (uint16_t)~I2C_IT_BUF;
                }
            }
            /* Master Receiver */
            else {
                /* Initialize Receive counter */
                Rx_Idx1 = 0;
                /* At this stage, ADDR is cleared because both SR1 and SR2 were read.*/
                /* EV6_1: used for single byte reception. The ACK disable and the STOP
                Programming should be done just after ADDR is cleared. */
                if (NumbOfBytes1 == 1) {
                    /* Clear ACK */
                    I2C1->CR1 &= ~I2C_CR1_ACK;
                    /* Program the STOP */
                    I2C1->CR1 |= I2C_CR1_STOP;
                }
            }
            SR1Register = 0;
            SR2Register = 0;

        }
        /* Master transmits the remaing data: from data2 until the last one.  */
        /* If TXE is set */
        if ((SR1Register &0x0084) == 0x0080) {
            /* If there is still data to write */
            if (NumbOfBytes1!=0) {
                /* Write the data in DR register */
                I2C1->DR = Buffer_Tx1[Tx_Idx1++];
                /* Decrment the number of data to be written */
                NumbOfBytes1--;
                /* If  no data remains to write, disable the BUF IT in order
                to not have again a TxE interrupt. */
                if (NumbOfBytes1 == 0) {
                    /* Disable the BUF IT */
                    I2C1->CR2 &= (uint16_t)~I2C_IT_BUF;
                }
            }
            SR1Register = 0;
            SR2Register = 0;
        }
        /* If BTF and TXE are set (EV8_2), program the STOP */
        if ((SR1Register &0x0084) == 0x0084) {
            /* Program the STOP */
            I2C1->CR1 |= I2C_CR1_STOP;
            /* Disable EVT IT In order to not have again a BTF IT */
            I2C1->CR2 &= (uint16_t)~I2C_IT_EVT;
            SR1Register = 0;
            SR2Register = 0;
        }
        /* If RXNE is set */
        if ((SR1Register &0x0040) == 0x0040) {
            /* Read the data register */
            Buffer_Rx1[Rx_Idx1++] = I2C1->DR;
            /* Decrement the number of bytes to be read */
            NumbOfBytes1--;
            /* If it remains only one byte to read, disable ACK and program the STOP (EV7_1) */
            if (NumbOfBytes1 == 1) {
                /* Clear ACK */
                I2C1->CR1 &= ~I2C_CR1_ACK;
                /* Program the STOP */
                I2C1->CR1 |= I2C_CR1_STOP;
            }
            SR1Register = 0;
            SR2Register = 0;
        }
    }
}

void I2C1_ER_IRQHandler(void) {

    __IO uint32_t SR1Register =0;

    /* Read the I2C1 status register */
    SR1Register = I2C1->SR1;
    /* If AF = 1 */
    if ((SR1Register & 0x0400) == 0x0400) {
        I2C1->SR1 &= 0xFBFF;
        SR1Register = 0;
    }
    /* If ARLO = 1 */
    if ((SR1Register & 0x0200) == 0x0200) {
        I2C1->SR1 &= 0xFBFF;
        SR1Register = 0;
    }
    /* If BERR = 1 */
    if ((SR1Register & 0x0100) == 0x0100) {
        I2C1->SR1 &= 0xFEFF;
        SR1Register = 0;
    }
    /* If OVR = 1 */
    if ((SR1Register & 0x0800) == 0x0800) {
        I2C1->SR1 &= 0xF7FF;
        SR1Register = 0;
    }
}

void I2C_DMAConfig(I2C_TypeDef* I2Cx, uint8_t* pBuffer, uint32_t BufferSize, uint32_t Direction)
{
    /* Initialize the DMA with the new parameters */
    if (Direction == I2C_DIRECTION_TX)
    {
        /* Configure the DMA Tx Channel with the buffer address and the buffer size */
        I2CDMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)pBuffer;
        I2CDMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
        I2CDMA_InitStructure.DMA_BufferSize = (uint32_t)BufferSize;

        I2CDMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)I2C1->DR;
        DMA_Cmd(DMA1_Channel6, DISABLE);
        DMA_Init(DMA1_Channel6, &I2CDMA_InitStructure);
        DMA_Cmd(DMA1_Channel6, ENABLE);
    }
    else /* Reception */
    {
        /* Configure the DMA Rx Channel with the buffer address and the buffer size */
        I2CDMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)pBuffer;
        I2CDMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
        I2CDMA_InitStructure.DMA_BufferSize = (uint32_t)BufferSize;

        I2CDMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)I2C1->DR;
        DMA_Cmd(DMA1_Channel7, DISABLE);
        DMA_Init(DMA1_Channel7, &I2CDMA_InitStructure);
        DMA_Cmd(DMA1_Channel7, ENABLE);
    }
}

Status I2C_Master_BufferWrite(I2C_TypeDef* I2Cx, uint8_t* pBuffer,  uint32_t NumByteToWrite, I2C_ProgrammingModel Mode, uint8_t SlaveAddress ) {

    __IO uint32_t temp = 0;
    __IO uint32_t Timeout = 0;

    /* Enable Error IT (used in all modes: DMA, Polling and Interrupts */
    I2Cx->CR2 |= I2C_IT_ERR;
    if (Mode == DMA)  /* I2Cx Master Transmission using DMA */
    {
        Timeout = 0xFFFF;
        /* Configure the DMA channel for I2Cx transmission */
        I2C_DMAConfig (I2Cx, pBuffer, NumByteToWrite, I2C_DIRECTION_TX);
        /* Enable the I2Cx DMA requests */
        I2Cx->CR2 |= I2C_CR2_DMAEN;
        /* Send START condition */
        I2Cx->CR1 |= I2C_CR1_START;
        /* Wait until SB flag is set: EV5 */
        while ((I2Cx->SR1&0x0001) != 0x0001) {
            if (Timeout-- == 0)
                return Error;
        }
        Timeout = 0xFFFF;
        /* Send slave address */
        /* Reset the address bit0 for write */
//        SlaveAddress &= OAR1_ADD0_Reset;
        Address = SlaveAddress;
        /* Send the slave address */
        I2Cx->DR = Address;
        /* Wait until ADDR is set: EV6 */
        while ((I2Cx->SR1&0x0002) != 0x0002) {
            if (Timeout-- == 0)
                return Error;
        }
        /* Clear ADDR flag by reading SR2 register */
        temp = I2Cx->SR2;
        /* Wait until DMA end of transfer */
        while (!DMA_GetFlagStatus(DMA1_FLAG_TC6));
        /* Disable the DMA1 Channel 6 */
        DMA_Cmd(DMA1_Channel6, DISABLE);
        /* Clear the DMA Transfer complete flag */
        DMA_ClearFlag(DMA1_FLAG_TC6);
        /* EV8_2: Wait until BTF is set before programming the STOP */
        while ((I2Cx->SR1 & 0x00004) != 0x000004);
        /* Program the STOP */
        I2Cx->CR1 |= I2C_CR1_STOP;
        /* Make sure that the STOP bit is cleared by Hardware */
        while ((I2Cx->CR1&0x200) == 0x200);
    }
    else if (Mode == Polling) /* I2Cx Master Transmission using Polling */
    {

        Timeout = 0xFFFF;
        /* Send START condition */
        I2Cx->CR1 |= I2C_CR1_START;
        /* Wait until SB flag is set: EV5 */
        while ((I2Cx->SR1&0x0001) != 0x0001) {
            if (Timeout-- == 0)
                return Error;
        }
        /* Send slave address */
        /* Reset the address bit0 for write*/
//        SlaveAddress &= OAR1_ADD0_Reset;
        Address = SlaveAddress;
        /* Send the slave address */
        I2Cx->DR = Address;
        Timeout = 0xFFFF;
        /* Wait until ADDR is set: EV6 */
        while ((I2Cx->SR1 &0x0002) != 0x0002) {
            if (Timeout-- == 0)
                return Error;
        }
        /* Clear ADDR flag by reading SR2 register */
        temp = I2Cx->SR2;
        /* Write the first data in DR register (EV8_1) */
        I2Cx->DR = *pBuffer;
        /* Increment */
        pBuffer++;
        /* Decrement the number of bytes to be written */
        NumByteToWrite--;
        /* While there is data to be written */
        while (NumByteToWrite--)
        {
            /* Poll on BTF to receive data because in polling mode we can not guarantee the
              EV8 software sequence is managed before the current byte transfer completes */
            while ((I2Cx->SR1 & 0x00004) != 0x000004);
            /* Send the current byte */
            I2Cx->DR = *pBuffer;
            /* Point to the next byte to be written */
            pBuffer++;
        }
        /* EV8_2: Wait until BTF is set before programming the STOP */
        while ((I2Cx->SR1 & 0x00004) != 0x000004);
        /* Send STOP condition */
        I2Cx->CR1 |= I2C_CR1_STOP;
        /* Make sure that the STOP bit is cleared by Hardware */
        while ((I2Cx->CR1&0x200) == 0x200);
    }
    else /* I2Cx Master Transmission using Interrupt with highest priority in the application */
    {
        /* Enable EVT IT*/
        I2Cx->CR2 |= I2C_IT_EVT;
        /* Enable BUF IT */
        I2Cx->CR2 |= I2C_IT_BUF;
        /* Set the I2C direction to Transmission */
        I2CDirection = I2C_DIRECTION_TX;
//        SlaveAddress &= OAR1_ADD0_Reset;
        Address = SlaveAddress;
        NumbOfBytes1 = NumByteToWrite;
        /* Send START condition */
        I2Cx->CR1 |= I2C_CR1_START;
        /* Wait until the START condition is generated on the bus: the START bit is cleared by hardware */
        while ((I2Cx->CR1&0x100) == 0x100);
        /* Wait until BUSY flag is reset: a STOP has been generated on the bus signaling the end
        of transmission */
        while ((I2Cx->SR2 &0x0002) == 0x0002);
    }
    return Success;
}

void I2C1_E_V_IRQHandler(void) {
uint8_t temp, temp2;
  
  temp  = I2C1->SR1;
  temp2 = I2C1->SR2;
  if ( temp & I2C_SR1_SB ) {
    (void) I2C1->SR1;
    I2C1->DR = SSD1306_I2C_ADDR;
  } else
  if ( temp & I2C_SR1_ADDR ) {
//    (void) I2C1->SR1;
//    (void) I2C1->SR2;
    I2C1->DR = ctrl;
  } else
  if ( temp & I2C_SR1_TXE ) {
//    if ( temp & I2C_SR1_BTF ) {
//      I2C1->CR1 |= I2C_CR1_STOP;
//    } else {
      if ( --len  )
        I2C1->DR = *data++;
  } else {
    if ( temp & I2C_SR1_BTF ) {
      I2C1->CR1 |= I2C_CR1_STOP;
//      if ( --len  )
//        I2C1->DR = *data++;
    }
  }
}
