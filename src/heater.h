#ifndef __HEATER_H__
#define __HEATER_H__

#include <stdbool.h>
#include <stdint.h>
#include "apptypes.h"
#include "hal/hal_gpio.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    HEATER_A 	= 0,
    HEATER_B 	= 1,
    HEATER_C 	= 2,
    HEATER_D 	= 3,
    HEATER_E 	= 4,
    MAX_HEATERS
}heater_id_t;

typedef enum
{
	ST_INACTIVE = 0,
	ST_ACTIVE   = 1,
    ST_SHORTED  = 2,
    ST_OPENED   = 3,
}heater_status_t;

typedef struct
{
	heater_id_t channel;
	uint16_t volts;
	uint16_t current;
	uint16_t resistance;
	int16_t temperature;
	uint8_t dutycycle;
	heater_status_t status;
}heat_params_t;

typedef struct
{
	heater_id_t id;
	IOPin heater_output;
	uint8_t addr_ina231;
	//logic levels to activate TEMP_A and TEMP_B before reading ADS
	uint8_t analog_mux_addr;
}heat_ch_config_t;

app_status_t hw_init(const heat_ch_config_t *config, uint8_t num_channels);
void hw_deinit(void);
void hw_stop(void);
uint32_t hw_get_duty_period_ms(void);
uint32_t hw_get_duty_period_count(void);
uint32_t hw_get_default_period_count(void);
app_status_t hw_set_duty_period(uint16_t period, bool mode_count);
bool hw_is_channel_enabled(heater_id_t ch);
bool hw_is_valid_voltage(uint8_t voltage);
bool hw_is_task_stopped(void);
uint8_t hw_get_highest_voltage(void);
uint8_t hw_get_lowest_voltage(void);
uint8_t hw_get_current_voltage(void);
app_status_t hw_get_pcb_temperature(uint16_t *pcbtemp);
app_status_t hw_set_lowest_vcc(void);
app_status_t hw_set_voltage(uint8_t voltage);
app_status_t hw_set_dutycycle(heater_id_t ch, uint8_t duty);
app_status_t hw_get_params(heater_id_t ch, heat_params_t *params);
uint32_t hw_get_ch_ontime_count(heater_id_t ch);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif //__HEATER_H__
