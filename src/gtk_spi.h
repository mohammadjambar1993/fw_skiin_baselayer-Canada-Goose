/*
 * gtk_spi.h
 *
 *  Created on: Sep 20, 2016
 *      Author: Myant
 */

#ifndef SRC_GTK_SPI_H_
#define SRC_GTK_SPI_H_

#include "appconfig.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */
#if GTK_SPI_ON == 1

#include "hal_config.h"
#include "hal_gpio.h"
#include "FreeRTOS.h"
#include "task.h"

#define IGNORE_CS_PIN   0xFF

bool gtk_spi_init(void);
void gtk_spi_deinit(void);
bool gtk_spi_tx(uint8_t *txd, uint8_t *rxd, uint16_t len, IOPin cs,
                TaskHandle_t *tsk);
bool gtk_is_configured(void);

#endif //GTK_SPI_ON

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_GTK_SPI_H_ */
