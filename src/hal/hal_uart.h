/*
 * hal_uart.h
 *
 *  Created on: Aug 22, 2016
 *      Author: Myant
 */

#ifndef SRC_HAL_HAL_UART_H_
#define SRC_HAL_HAL_UART_H_

#include "hal_config.h"
#include "hal_clock.h"
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833)
#include "nrf_drv_uart.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    UARTST_OP_OK,
    UARTST_OP_FAIL,
    UARTST_CALLB_FULL,
    UARTST_CALLB_INVALID,
}UartStatus;

typedef enum
{
    UARTEVT_TXEND,
    UARTEVT_RXEND,
    UARTEVT_ERROR,
}UartEvent;

typedef void(*uart_callb_t)(UartEvent evt, uint8_t *data, uint16_t len);

UartStatus hal_uart_init(void);
void hal_uart_deinit(void);
void hal_uart_enable(bool en);
void hal_uart_enableIRQ_RX(bool en);
void hal_uart_enableIRQ_TX(bool en);
void hal_uart_enable_globalIRQ(bool en);
void hal_uart_tx(uint8_t *data, uint16_t len);
void hal_uart_tx_blocking(uint8_t *data, uint16_t len);
void hal_uart_flush_rx(void);
uint32_t hal_uart_len_tx_buffer(void);
uint32_t hal_uart_get_baudrate(void);
UartStatus hal_uart_addCallback(uart_callb_t rxcall);
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833)
	const nrf_drv_uart_config_t *hal_uart_get_config(void);
	UartStatus hal_uart_init_extern(nrf_drv_uart_config_t *config);
#endif

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_HAL_HAL_UART_H_ */
