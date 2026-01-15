/*
 * mode_stream.h
 *
 *  Created on: Aug 24, 2017
 *      Author: Myant
 */

#ifndef MODE_STREAM_H_
#define MODE_STREAM_H_

#include <stdbool.h>
#include <stdint.h>
#include "appconfig.h"
#include "apptypes.h"
#include "cmd_protocol.h"
#include "heater.h"
#include "bandcfg.h"
#include "ble.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef struct
{
	temp_ctl_t ctl_settings;		//settings for temperature control
	heat_params_t measures;			//low level measures from heat channel
	int16_t temperature_setpoint;	//temperature setpoint
	bool overheating;				//if channel temperature is above the limit
}channel_ctl_t;

app_status_t heat_init(void);
app_status_t heat_set_channels(cmd_heat_params_t *params);
app_status_t heat_set_temperature_sp(cmd_heat_params_t *params);
bool heat_is_channel_overheating(heater_id_t ch);
bool heat_is_hardware_failed(void);
void heat_cycle_printing(void);
void heat_stop_control(void);
bool heat_get_channel_params(heater_id_t ch, channel_ctl_t *params);
uint16_t heat_get_remaining_session_time(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* MODE_STREAM_H_ */
