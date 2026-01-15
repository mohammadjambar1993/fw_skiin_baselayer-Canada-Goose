/*
 * mx25r6435.h
 *
 *  Created on: Sep 20, 2017
 *      Author: Myant
 */

#ifndef SRC_DRIVERS_MX25R6435_H_
#define SRC_DRIVERS_MX25R6435_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "hal_gpio.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#define CLI_SUPPORT_ON  1

typedef enum
{
    MEMST_OK            = 0,
    MEMST_FAIL          = 1,
    MEMST_LENGTH_ERROR  = 2,
    MEMST_ADDR_ERROR    = 3,
    MEMST_WR_DISABLED   = 4,
    MEMST_INVALID_PARAM = 5,
    MEMST_BUSY          = 6,
    MEMST_SPI_ERROR     = 7,
}memory_status_t;

//pointers to functions that should be implemented externally
typedef bool(*spi_tx_f)(uint8_t *txd, uint8_t ntx, uint8_t *rxd,
		uint8_t nrx,uint8_t cs);
typedef void(*delay_f)(uint32_t delay);
typedef void(*pin_clear_f)(IOPin pin);
typedef void(*pin_set_f)(IOPin pin);

typedef void *ads93_t;

typedef struct
{
    spi_tx_f spi_tx;
    delay_f delay_ms;
    uint8_t cs;
    pin_clear_f pin_clr;
    pin_set_f pin_set;
}mx_hw_t;

memory_status_t mx_init(mx_hw_t *hw);
memory_status_t mx_read_id(uint16_t *id);
memory_status_t mx_read_status(uint8_t *status);
memory_status_t mx_read_page(uint16_t page, uint32_t addr, void *data, uint16_t len);
memory_status_t mx_read(uint32_t addr, void *data, uint16_t len);
memory_status_t mx_write(uint32_t addr, void *data, uint16_t len);
memory_status_t mx_write_page(uint16_t page, uint16_t addr, void *data, uint16_t len);
memory_status_t mx_erase_page(uint32_t addr);
memory_status_t mx_power_down(void);

bool mx_is_busy(void);
#if CLI_SUPPORT_ON == 1
memory_status_t mx_set_trace(bool enabled);
#endif //CLI_SUPPORT_ON

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_DRIVERS_MX25R6435_H_ */
