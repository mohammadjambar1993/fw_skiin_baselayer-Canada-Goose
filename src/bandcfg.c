/*
 * bandcfg.c
 *
 *  Created on: Oct. 7, 2021
 *      Author: tmg
 */

#include <string.h>
#include "bandcfg.h"
#include "heater.h"
#include "heat_ctrl.h"
#include "crc16.h"
#include "hal/hal_gpio.h"
#include "util/mya_util.h"
#include "drivers/mx25r6435.h"
#include "memory.h"

#define ENABLE_LOGGER_BANDCFG
#ifdef ENABLE_LOGGER_BANDCFG
	#include "util/logger.h"
	#define logtag	"[band] "
	#define _debug(...) 	log_debug(logtag __VA_ARGS__)
	#define _info(...)		log_info(logtag __VA_ARGS__)
	#define _warn(...)		log_warn(logtag __VA_ARGS__)
	#define _error(...)		log_error(logtag __VA_ARGS__)
#else
	#define _debug
	#define _info
	#define _warn
	#define _error
#endif

static void reset_ctl_params(void);
static void load_tempctl_params(void);
static void set_garment_id(void);
static void print_temp_ctrl(temp_ctl_settings_t *params);
static void turn_heat_off(void);

static bool initialized = false;
static bool writing_params = false;
static garment_id_t garment = GID_UNKNOWN;
static temp_ctl_t ctl_params[MAX_HEATERS] = {0};

bool band_init(void)
{
	if(initialized)
	{
		return false;
	}
	load_tempctl_params();
	set_garment_id();
	initialized = true;
	writing_params = false;

	return initialized;
}

garment_id_t band_get_garmentid(void)
{
	return garment;
}

static void set_garment_id(void)
{
	const uint8_t ACTIVE_CH_DELILAH_V1 = 0b00011000; //Channels D and E
	const uint8_t ACTIVE_CH_DELILAH_V2 = 0b00001100; //Channels C and D
	const uint8_t ACTIVE_CH_LOWER_BACK = 0b00000110; //Channels B and C

	uint8_t active_channels = 0;
	temp_ctl_t *param = &ctl_params[0];

	//get active channels
	for(uint i = HEATER_A; i < MAX_HEATERS; i++, param++)
	{
		if(param->enabled)
			set_bit(active_channels, i);
	}

	//set garment id based on active channels
	if(ACTIVE_CH_DELILAH_V1 == active_channels)
		garment = GID_DELILAH;
	else if(ACTIVE_CH_DELILAH_V2 == active_channels)
		garment = GID_DELILAH;
	else if(ACTIVE_CH_LOWER_BACK == active_channels)
		garment = GID_LOWER_BACK;
	else
		garment = GID_UNKNOWN;
}

static void reset_ctl_params(void)
{
	memset(ctl_params, 0, sizeof(ctl_params));
	for(uint i = HEATER_A; i < MAX_HEATERS; i++)
	{
		ctl_params[i].low.type = TEMP_CTRL_NONE;
		ctl_params[i].medium.type = TEMP_CTRL_NONE;
		ctl_params[i].high.type = TEMP_CTRL_NONE;
		ctl_params[i].channelid = (heater_id_t)i;
		ctl_params[i].enabled = false;
	}
}

#define PARAMS_ADDR		0
#define PARAMS_PAGE		0
#define LEN_BUFFER		512
#define LEN_WRITE_OP	64 //maximum bytes to write in a single flash operation
#define LEN_READ_OP		66 //maximum bytes to read in a single flash operation
static uint8_t params_buffer[LEN_BUFFER] = {0};
static void load_tempctl_params(void)
{
	uint16_t crc;
	memory_status_t st;
	temp_ctl_t *param;

	reset_ctl_params();
	for(uint i = 0; i < MAX_HEATERS; i++)
	{
		st = mem_read(i*LEN_READ_OP, &params_buffer[i*LEN_READ_OP],LEN_READ_OP);
		if(MEMST_OK != st)
		{
			_error("failed to read ch[%d] params data",i);
			break;
		}
	}

	if(MEMST_OK != st)
	{
		reset_ctl_params();
		_error("failed to read parameters %d", st);
		return;
	}
	memcpy(ctl_params, params_buffer, sizeof(ctl_params));
	//check data integrity
	param = &ctl_params[0];
	for(uint i = HEATER_A; i < MAX_HEATERS; i++, param++)
	{
		crc = param->crc;
		param->crc = 0;
		param->crc = crc16_compute((const uint8_t*)param, sizeof(temp_ctl_t), NULL);
		param->enabled = (crc == param->crc);
		// Set temporary values if no parameters are sent over BLE
		if(!param->enabled)
		{
			memset(&ctl_params[i], 0xFF, sizeof(temp_ctl_t));
			ctl_params[i].low.type = TEMP_CTRL_NONE;
			ctl_params[i].medium.type = TEMP_CTRL_NONE;
			ctl_params[i].high.type = TEMP_CTRL_NONE;
			ctl_params[i].channelid = (heater_id_t)i;
			ctl_params[i].enabled = true;
		}
		_info("channel %d params enabled: %d", i, param->enabled);
	}
}

static void turn_heat_off(void)
{
	cmd_heat_params_t cmd;
	app_status_t st;

	memset(&cmd, 0, sizeof(cmd_heat_params_t));
	cmd.pwm_period = hw_get_default_period_count();
	cmd.voltage = hw_get_lowest_voltage();

	st = heat_set_channels(&cmd);
	if(APPST_SUCCESS != st)
	{
		_warn("Failed to turn heater off");
	}
}

app_status_t band_get_ctl_settings(heater_id_t ch, temp_ctl_t *settings)
{
	if((ch > MAX_HEATERS) || (NULL == settings))
		return APPST_INVALID_PARAM;
	memcpy(settings, &ctl_params[ch], sizeof(temp_ctl_t));
	return APPST_SUCCESS;
}

app_status_t band_store_params(void)
{
	uint32_t addr, length;
	memory_status_t st;
	temp_ctl_t *param;

	_info("Writing band parameters in flash");
	turn_heat_off();
	writing_params = false;

	addr = PARAMS_ADDR;
	st = mem_erase_page(addr);
	if(MEMST_OK != st)
	{
		_error("failed to erase sector %d, %d", addr, st);
		return APPST_ERROR;
	}
	util_blocking_delay_ms(100); //erase page takes ~85ms to complete
	//copy buffer to parmaters array
	memcpy(ctl_params, params_buffer, sizeof(ctl_params));
	//calculate crc for each channel parameter
	param = &ctl_params[0];
	length = sizeof(temp_ctl_t);
	for(uint i = HEATER_A; i < MAX_HEATERS; i++, param++)
	{
		param->crc = 0;
		param->channelid = i;
		if(param->enabled)
		{
			param->crc = crc16_compute((const uint8_t*)param, length, NULL);
		}
	}
	//store in flash
	memset(params_buffer, 0xFF, sizeof(params_buffer));
	memcpy(params_buffer, ctl_params, sizeof(ctl_params));
	for(uint i = 0; i < (LEN_BUFFER/LEN_WRITE_OP); i++)
	{
		st = mem_write(addr, &params_buffer[i*LEN_WRITE_OP], LEN_WRITE_OP);
		addr += LEN_WRITE_OP;
		if(MEMST_OK != st)
		{
			_error("failed to write params data");
			break;
		}
		util_blocking_delay_ms(20);
	}
	return APPST_SUCCESS;
}

app_status_t band_write_params(band_config_t *params)
{
	uint16_t index;
	uint8_t *buffer;

	if(!writing_params)
	{
		memset(params_buffer, 0xFF, sizeof(params_buffer));
		memset(params_buffer, 0, sizeof(temp_ctl_t));
		writing_params = true;
	}
	if(params->id >= MAX_HEATERS)
	{
		_error("invalid band id: %d", params->id);
		return APPST_INVALID_PARAM;
	}
	if(params->index+params->length > sizeof(temp_ctl_t))
	{
		_error("band write overflow: %d", params->index+params->length);
		return APPST_INVALID_PARAM;
	}

	index = params->id*sizeof(temp_ctl_t)+params->index;
	buffer = &params_buffer[index];
	memcpy(buffer, params->data, params->length);

	return APPST_SUCCESS;
}

void band_print_params(void)
{
	temp_ctl_t *params = ctl_params;
	for(uint8_t i = 0; i < MAX_HEATERS; i++, params++)
	{
		_info("--- Channel %d: %d", params->channelid, params->enabled);
		if(!params->enabled)
			continue;
		_info("temp cut off...: %d", params->temp_cutoff);
		_info("r conv coeff...: %d", params->r_conv_coeff);
		_info("r calibration..: %d", params->r_calibration);
		_info("- Low temperature settings");
		print_temp_ctrl(&params->low);
		os_delay_ms(50);
		_info("- Medium temperature settings");
		print_temp_ctrl(&params->medium);
		os_delay_ms(50);
		_info("- High temperature settings");
		print_temp_ctrl(&params->high);
		_info("---");
	}
}

static void print_temp_ctrl(temp_ctl_settings_t *params)
{
	if(params->type == TEMP_CTRL_NONE)
	{
		_info("Control type...: None");
		return;
	}
	else if(params->type == TEMP_CTRL_ONOFF)
	{
		_info("Control type...: on/off");
		_info("temp setpoint..: %d", params->temperature_sp);
		_info("resist. low....: %d", params->resistance_lower);
		_info("resist. high...: %d", params->resistance_upper);
		_info("battery vcc....: %d", params->battery_vcc);
		_info("duty minimum...: %d", params->duty_min);
		_info("duty maximum...: %d", params->duty_max);
	}
	else if(params->type == TEMP_CTRL_PI)
	{
		_info("Control type...: pi");
		_info("temp setpoint..: %d", params->temperature_sp);
		_info("battery vcc....: %d", params->battery_vcc);
		_info("resistance.....: %d", params->resistance_pi);
		_info("duty minimum...: %d", params->duty_min);
		_info("duty maximum...: %d", params->duty_max);
		_info("kp, ki.........: %d, %d", params->kp, params->ki);
		_info("err min, max...: %d, %d", params->ierror_min, params->ierror_max);
	}
	else
	{
		_info("Control type...: invalid (%d)", params->type);
	}
}
