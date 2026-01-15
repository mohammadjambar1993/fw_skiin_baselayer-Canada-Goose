/*
 * bandcfg.h
 *
 *  Created on: Oct. 7, 2021
 *      Author: tmg
 */

#ifndef SRC_BANDCFG_H_
#define SRC_BANDCFG_H_

#include <stdbool.h>
#include <stdint.h>
#include "appconfig.h"
#include "apptypes.h"
#include "heater.h"
#include "cmd_protocol.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    GID_UNKNOWN   	= 0,
	GID_DELILAH    	= 6,
	GID_LOWER_BACK	= 7
}garment_id_t;

typedef enum   //added for temperature control mode
{
	TEMP_CTRL_PI 		= 0,	//has temperature control above this mode
	TEMP_CTRL_NONE		= 1,	//no temperature control
	TEMP_CTRL_ONOFF		= 2,	//on/off temperature control
}heat_ctl_mode_t;

typedef struct
{
	//type: defines which temperature control algorithm is going to be used
	heat_ctl_mode_t type;
	uint8_t temperature_sp;		//temperature setpoint
	uint16_t resistance_lower;	//lower bound resistance (scaled x100)
	uint16_t resistance_upper;	//upper bound resistance (scaled x100)
	uint8_t battery_vcc;		//VCC to be applied to heating element (scaled x10)
	uint8_t duty_min;			//min duty cycle
	uint8_t duty_max;			//max duty cycle
	//parameters below are used only for CTRL_PI type
	uint16_t resistance_pi;		//resistance constant 	(scaled x100)
	int16_t kp;					//proportional constant (scaled x100)
	int16_t ki;					//integral constant		(scaled x100)
	uint16_t ierror_min;		//integral error min (scaled x100)
	uint16_t ierror_max;		//integral error max (scaled x100)
}__attribute__((packed, aligned(1))) temp_ctl_settings_t;

typedef struct
{
	//indicates if this channel is used by the firmware or not
	bool enabled;
	uint16_t crc;			//used to check data integrity when stored in flash
	//if temperature is greater than the cutoff, outputs will be turned off
	uint8_t temp_cutoff;
	heater_id_t channelid;
	uint16_t r_conv_coeff;		//scaled x100
	uint16_t r_calibration;		//scaled x100
	temp_ctl_settings_t low;
	temp_ctl_settings_t medium;
	temp_ctl_settings_t high;
}__attribute__((packed, aligned(1))) temp_ctl_t;

bool band_init(void);
void band_print_params(void);
garment_id_t band_get_garmentid(void);
app_status_t band_get_ctl_settings(heater_id_t ch, temp_ctl_t *settings);
app_status_t band_write_params(band_config_t *params);
app_status_t band_store_params(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_BANDCFG_H_ */
