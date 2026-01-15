/*
 * hal_i2c.h
 *
 *  Created on: Aug 30, 2016
 *      Author: Myant
 */

#ifndef SRC_HAL_HAL_I2C_H_
#define SRC_HAL_HAL_I2C_H_

#include "hal_config.h"
#include "hal_clock.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    I2CST_OP_OK,
    I2CST_OP_FAIL,
    I2CST_CALLB_FULL,
    I2CST_CALLB_INVALID,
    I2CST_IRQ_OP_DONE,
    I2CST_IRQ_ADDR_NACK,
    I2CST_IRQ_DATA_NACK,
    I2CST_BUSY,
}I2CStatus;

I2CStatus hal_i2c_init(void);
void hal_i2c_enable(bool en);
void hal_i2c_enableIRQ(bool en);
I2CStatus hal_i2c_tx(uint8_t addr, uint8_t *txdata, uint16_t len, bool stop);
I2CStatus hal_i2c_rx(uint8_t addr, uint8_t *rxdata, uint16_t len);
I2CStatus hal_i2c_set_irq_callback(void(*callb)(I2CStatus st));
uint32_t hal_i2c_len_tx_buffer(void);
uint32_t hal_i2c_len_rx_buffer(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */


#endif /* SRC_HAL_HAL_I2C_H_ */
