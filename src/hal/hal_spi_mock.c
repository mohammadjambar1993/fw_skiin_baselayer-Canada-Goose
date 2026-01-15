/*
 * hal_spi_mock.c
 *
 *  Created on: Aug 31, 2016
 *      Author: Myant
 */

#include "hal_spi.h"
#if ENABLE_HAL_SPI == 0

SPIStatus hal_spi_init(void)
{
    return SPIST_OP_FAIL;
}

void hal_spi_enable(bool en)
{

}

void hal_spi_enableIRQ_TX(bool en)
{

}

void hal_spi_tx(uint8_t *txdata, uint8_t *rxdata, uint16_t len)
{

}

SPIStatus hal_spi_addTXEndCallback(void(*txend)(void))
{
    return SPIST_CALLB_INVALID;
}

#endif //ENABLE_HAL_SPI == 0
