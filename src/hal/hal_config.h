/*
 * hal_config.h
 *
 *  Created on: Jul 26, 2016
 *      Author: Myant
 */

#ifndef SOURCE_HAL_HAL_CONFIG_H_
#define SOURCE_HAL_HAL_CONFIG_H_

//list of supported microcontrollers
#define UC_MK22FX512   0
#define UC_NRF52832    1
#define UC_NRF52833    2
//select current microcontroller (uC)
#define UC_ID          UC_NRF52833

//select development kits
//If a development kit is used, activate special code
#if UC_ID == UC_NRF52832
    #define DEVKIT_NRF52    0
#endif

//based on selected uC define the SDK (used mainly by HAL)
#if (UC_ID == UC_MK22FX512)
#define KINETIS_SDK20
#include "fsl_common.h"
#endif //UCMK22FX512
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833)
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#endif //UC_NRF52832

//constants to enable/disable peripherals
#define ENABLE_HAL_UART     0
#define ENABLE_HAL_SPI      1
#define ENABLE_HAL_I2C      1
#define ENABLE_HAL_BLE      1
#define ENABLE_HAL_TIMER    0

//Function prototypes
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

void hal_config_init(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SOURCE_HAL_HAL_CONFIG_H_ */
