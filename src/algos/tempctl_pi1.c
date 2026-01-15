/*
 * tempctl_pi1.c
 *
 *  Created on: Oct. 15, 2021
 *      Author: tmg
 */

#include <string.h>
#include "autotempctl.h"
#include "temperature_ads.h"

#if TEMP_CTL_ALGORITHM == TEMP_CONTROL_PI_1

//used to hold the output values from the algorithm
static cmd_heat_params_t outparams;

static uint8_t ctl_temp_pi(const heat_params_t *params,
						const temp_ctl_settings_t *settings);
static uint8_t ctl_temp_onoff(const heat_params_t *params,
						   const temp_ctl_settings_t *settings);

app_status_t tctl_init(void)
{
	outparams.voltage = 0;
	outparams.pwm_period = 0;
	memset(outparams.data, INVALID_DUTY_CYCLE, sizeof(outparams.data));

	return APPST_SUCCESS;
}

app_status_t tctl_run(const channel_ctl_t *channels)
{
	uint8_t dutycycle;
	const channel_ctl_t *ch = channels;
	for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
	{
		if(!ch->ctl_settings.enabled)
		{
			outparams.data[i] = INVALID_DUTY_CYCLE;
			continue; //next channel
		}
		dutycycle = ch->measures.dutycycle;
		if(ch->temperature_setpoint == 0) //0 is channel turned off
		{
			dutycycle = 0;
		}
		else if(ch->temperature_setpoint >= ch->ctl_settings.high.temperature_sp)
		{
			if(TEMP_CTRL_ONOFF == ch->ctl_settings.high.type)
				dutycycle = ctl_temp_onoff(&ch->measures, &ch->ctl_settings.high);
			else if(TEMP_CTRL_PI == ch->ctl_settings.high.type)
				dutycycle = ctl_temp_pi(&ch->measures, &ch->ctl_settings.high);
		}
		else if(ch->temperature_setpoint == ch->ctl_settings.medium.temperature_sp)
		{
			if(TEMP_CTRL_ONOFF == ch->ctl_settings.medium.type)
				dutycycle = ctl_temp_onoff(&ch->measures, &ch->ctl_settings.medium);
			else if(TEMP_CTRL_PI == ch->ctl_settings.medium.type)
				dutycycle = ctl_temp_pi(&ch->measures, &ch->ctl_settings.medium);
		}
		else if(ch->temperature_setpoint == ch->ctl_settings.low.temperature_sp)
		{
			if(TEMP_CTRL_ONOFF == ch->ctl_settings.low.type)
				dutycycle = ctl_temp_onoff(&ch->measures, &ch->ctl_settings.low);
			else if(TEMP_CTRL_PI == ch->ctl_settings.low.type)
				dutycycle = ctl_temp_pi(&ch->measures, &ch->ctl_settings.low);
		}
		//set control parameters
		outparams.data[i] = dutycycle;
	}
	outparams.pwm_period = hw_get_default_period_count();
	outparams.voltage = hw_get_highest_voltage();
	return APPST_SUCCESS;
}

app_status_t tctl_get_result(cmd_heat_params_t *params)
{
	if(NULL == params)
		return APPST_INVALID_PARAM;
	memcpy(params, &outparams, sizeof(cmd_heat_params_t));
	return APPST_SUCCESS;
}

static uint8_t ctl_temp_onoff(const heat_params_t *params,
						   const temp_ctl_settings_t *settings)
{
	uint8_t newduty = params->dutycycle;

	if(params->resistance < settings->resistance_lower)
	{
		newduty = settings->duty_max;
	}
	else if(params->resistance > settings->resistance_upper)
	{
		newduty = settings->duty_min;
	}
	return newduty;
}

static uint8_t ctl_temp_pi(const heat_params_t *params,
						const temp_ctl_settings_t *settings)
{
	static int16_t sum_error = 0;
	int16_t p_error, i_error;
	int32_t output;
	uint8_t newduty;

	p_error = (settings->resistance_pi)-(params->resistance);
	i_error = sum_error + p_error;
	if(i_error >= settings->ierror_min && i_error <= settings->ierror_max)
	{
		sum_error = i_error;
	}
	output  = (settings->kp*((int32_t)p_error));
	output += (settings->ki*((int32_t)sum_error));
	output = output/100;
	if(output > settings->duty_max)
	{
		newduty = settings->duty_max;
	}
	else if(output < 0)
	{
		newduty = 0;
	}
	else
	{
		newduty = output;
	}
	return newduty;
}

#endif //TEMP_CTL_ALGORITHM == TEMP_CONTROL_PI_1

