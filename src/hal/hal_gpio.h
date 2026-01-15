#ifndef SOURCE_HAL_HAL_GPIO_H_
#define SOURCE_HAL_HAL_GPIO_H_

#include "hal_config.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    IOST_MAP_OK,
    IOST_MAP_ERROR,
}IOStatus;

typedef enum
{
    //outputs
    LED_R       		=0,
    LED_G,
    LED_B,
	MEM_RST,
	CS_MEM,
    CS_IMU,
	IMU_INT,
	HEATING_CHA,
	HEATING_CHB,
	HEATING_CHC,
	HEATING_CHD,
	HEATING_CHE,
	PD_RESET,
	V_HEATER_EN,
	TEMP_A,
	TEMP_B,
	TEMP_EN,
	TEMP_E_EN,
	TEMP_SW,
	CS_ADS,
	DRDY,
	REF_R1,
	REF_R2,
	REF_R3,
	REF_R4,
	REF_R5,
	REF_R6,
	REF_R7,
	BUTTON,
	MAX_IOPINS,
}IOPin;

typedef void(*pinirq_callb_t)(void);

IOStatus hal_gpio_init(void);
void hal_gpio_deinit(void);
void hal_gpio_set(IOPin pin);
void hal_gpio_clr(IOPin pin);
void hal_gpio_toggle(IOPin pin);
void hal_gpio_write(IOPin pin, uint8_t level);
uint32_t hal_gpio_read(IOPin pin);
bool hal_gpio_enable_irq(IOPin pin, bool en);
bool hal_gpio_irq_callb(IOPin pin, pinirq_callb_t callb);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SOURCE_HAL_HAL_GPIO_H_ */
