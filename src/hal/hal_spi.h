/*
 * hal_spi.h
 *
 *  Created on: Aug 25, 2016
 *      Author: Myant
 */

#ifndef SRC_HAL_HAL_SPI_H_
#define SRC_HAL_HAL_SPI_H_

#include "hal_config.h"
#include "hal_clock.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    SPI_ID1 = 0,
    SPI_ID2 = 1,
    //do not define new spi ports below this point
    MAX_SPI_PORTS
}SPIId_e;

typedef enum
{
    SPIST_OP_OK,
    SPIST_OP_FAIL,
    SPIST_CALLB_FULL,
    SPIST_CALLB_INVALID,
    SPIST_INVALID_PORT,
    SPIST_BUSY,
}SPIStatus;

typedef void * spi_port_t;
typedef void (*spi_tx_callback_t)(void);

spi_port_t hal_spi_init(SPIId_e id);
void hal_spi_deinit(SPIId_e id);
bool hal_spi_is_enabled(spi_port_t spi);
void hal_spi_enable(spi_port_t spi, bool en);
void hal_spi_enable_irq_tx(spi_port_t spi, bool en);
SPIStatus hal_spi_tx(spi_port_t spi, uint8_t *txdata, uint8_t *rxdata, uint16_t len);
SPIStatus hal_spi_set_tx_callback(spi_port_t spi, spi_tx_callback_t callb);
uint32_t hal_spi_len_tx_buffer(spi_port_t spi);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_HAL_HAL_SPI_H_ */
