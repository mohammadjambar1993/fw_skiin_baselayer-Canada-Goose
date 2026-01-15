/*
 * ble_commands.c
 *
 *  Created on: Oct. 4, 2021
 *      Author: tmg
 */

#include <string.h>
#include "logger.h"
#include "ble_commands.h"
#include "cmd_protocol.h"
#include "heat_ctrl.h"
#include "temperature_ads.h"

#define ENABLE_LOGGER_BLECMDS
#ifdef ENABLE_LOGGER_BLECMDS
	#include "util/logger.h"
	#define logtag	"[cmd] "
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

static void cmd_read_serial_number(void);
static void cmd_write_serial_number(void);
static void cmd_set_heat_output(void);
static void cmd_set_heat_temp(void);
static void cmd_reset_module(void);
static void cmd_enter_shipmode(void);
static void cmd_dump_diag(void);
static void cmd_set_sens_mode(void);
static void cmd_read_band_params(void);
static void cmd_write_band_params(void);
static void cmd_store_band_params(void);
static void cmd_print_band_params(void);
static void cmd_write_encryption_key(void);
static void cmd_read_encryption_key(void);
static void cmd_write_secret_number(void);
static void cmd_read_secret_number(void);
static void cmd_activation_timeout(void);
static void cmd_write_ads_config(void);
static void cmd_read_ads_config(void);

/* list of commands */
static const command_t commands[] =
{
	{OPC_DUMP_DIAGNOSTIC, 		cmd_dump_diag},
	{OPC_WRITE_SERIAL_NUMBER, 	cmd_write_serial_number},
	{OPC_READ_SERIAL_NUMBER, 	cmd_read_serial_number},
    {OPC_WRITE_ENCRYPTION_KEY,  cmd_write_encryption_key},
    {OPC_READ_ENCRYPTION_KEY,   cmd_read_encryption_key},
    {OPC_WRITE_SECRET_NUMBER,   cmd_write_secret_number},
    {OPC_READ_SECRET_NUMBER,    cmd_read_secret_number},
	{OPC_SET_HEAT_POWER, 		cmd_set_heat_output},
	{OPC_RESET_MODULE, 			cmd_reset_module},
	{OPC_ENTER_SHIPMODE, 		cmd_enter_shipmode},
	{OPC_SET_HEAT_TEMP, 		cmd_set_heat_temp},
	{OPC_ACTIVE_TIMEOUT, 		cmd_activation_timeout},
	{OPC_WRITE_ADS_REGS, 		cmd_write_ads_config},
	{OPC_READ_ADS_REGS, 		cmd_read_ads_config},
	/*for hardware engineer to config ads1210*/
	{OPC_SET_SENS_MODE, 		cmd_set_sens_mode},
	{OPC_READ_BAND_PARAMS, 		cmd_read_band_params},
	{OPC_WRITE_BAND_PARAMS, 	cmd_write_band_params},
	{OPC_PERSIST_BAND_PARAMS, 	cmd_store_band_params},
	{OPC_PRINT_BAND_PARAMS, 	cmd_print_band_params},
};

static const uint16_t NCOMMANDS = sizeof(commands)/sizeof(command_t);
static cmdflag_t blecmd;

app_status_t blecmd_init(void)
{
	memset(&blecmd, 0, sizeof(blecmd));
	return APPST_SUCCESS;
}

static cmd_status appst2commandst(app_status_t appst)
{
	cmd_status cmdst;
	switch(appst)
	{
		case APPST_SUCCESS:
			cmdst = CMDST_OK;
		break;
		case APPST_ERROR:
		case APPST_OS_ERROR:
			cmdst = CMDST_ERROR;
		break;
		case APPST_INVALID_STATE:
			cmdst = CMDST_INV_STATE;
		break;
		case APPST_INVALID_PARAM:
			cmdst = CMDST_INV_DATA;
		break;
		case APPST_INVALID_LENGTH:
			cmdst = CMDST_INV_LEN;
		break;
		default:
			cmdst = CMDST_ERROR;
	}
	return cmdst;
}

void blecmd_process(void)
{
    uint16_t i;
    cmd_status st;
    bool cmdprocessed = false;
    const command_t *cmdptr = commands;

    if(blecmd.newcmd) //last command was not processed yet
    {
        _warn("Last BLE cmd not processed yet");
        return;
    }
    st = cmd_get_command(&blecmd); //get command
    if(CMDST_OK != st)
    {
        _warn("error reading command char: %d", st);
        cmd_send_response(blecmd.opcode, st, NULL, 0);
        return;
    }
    /*execute command*/
    for(i = 0; i < NCOMMANDS; i++, cmdptr++)
    {
        if(blecmd.opcode == cmdptr->opcode)
        {
            cmdptr->parser();
            cmdprocessed = true;
            break;
        }
    }
    if(!cmdprocessed)
        cmd_send_response(blecmd.opcode, CMDST_INV_OPCODE, NULL, 0);
    blecmd.newcmd = false;
}

static void cmd_read_serial_number(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_write_serial_number(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_set_heat_output(void)
{
	app_status_t appst;
	cmd_heat_params_t params;
	cmd_status st = cmd_get_heating_params(&blecmd, &params);
	if(CMDST_OK == st)
	{
		appst = heat_set_channels(&params);
		st = appst2commandst(appst);
	}
	cmd_send_response(blecmd.opcode, st, NULL, 0);
}

static void cmd_set_heat_temp(void)
{
	app_status_t appst;
	cmd_heat_params_t params;
	cmd_status st = cmd_get_temp_setpoint(&blecmd, &params);
	if(CMDST_OK == st)
	{
		appst = heat_set_temperature_sp(&params);
		st = appst2commandst(appst);
	}
	cmd_send_response(blecmd.opcode, st, NULL, 0);
}


static void cmd_write_ads_config(void)
{
#if ENABLE_ADS_TEST == 0
	cmd_send_response(blecmd.opcode, CMDST_INV_OPCODE, NULL, 0);

#elif ENABLE_ADS_TEST == 1
	app_status_t appst;
	cmd_ads_config_t config;

	if(!hw_is_task_stopped()) //stop tasks that might be accessing the SPI port
	{
		heat_stop_control();
		os_delay_ms(3000); //give some time to make sure the task is really stopped
	}
	if(!hw_is_task_stopped())
		cmd_send_response(blecmd.opcode, CMDST_INV_STATE, NULL, 0);

	cmd_status st = cmd_get_ads_config(&blecmd, &config);
	if(CMDST_OK == st)
	{
		appst = ads_write_config(&config);
		st = appst2commandst(appst);
	}
	cmd_send_response(blecmd.opcode, st, NULL, 0);
	os_delay_ms(1000);
	// System reset once ADS config has been written, VM command not working
	// for reset.
	if(CMDST_OK == st)
	{
		__disable_irq();
		NVIC_SystemReset();
	}
#endif
}

static void cmd_read_ads_config(void)
{
	cmd_ads_config_t config_r;
	app_status_t appst = ads_read_config(&config_r);
	cmd_status st = appst2commandst(appst);

	cmd_send_response(blecmd.opcode, st, &config_r, sizeof(config_r));
}

/*<<review.piyush.1A>>
* This command will be removed in future release to avoid potential bugs.
* Need to confirm with the Software team.
* */
static void cmd_reset_module(void)
{
	cmd_send_response(blecmd.opcode, CMDST_OK, NULL, 0);
	__disable_irq();
	NVIC_SystemReset();
}

static void cmd_enter_shipmode(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_dump_diag(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_set_sens_mode(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_read_band_params(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_write_band_params(void)
{
	cmd_status st;
	app_status_t appst;
	band_config_t band;

	st = cmd_get_band_params(&blecmd, &band);
	if(CMDST_OK == st)
	{
		appst = band_write_params(&band);
		st = appst2commandst(appst);
	}
	cmd_send_response(blecmd.opcode, st, NULL, 0);
}

static void cmd_store_band_params(void)
{
	cmd_status st = appst2commandst(band_store_params());
	cmd_send_response(blecmd.opcode, st, NULL, 0);
}

static void cmd_print_band_params(void)
{
	cmd_send_response(blecmd.opcode, CMDST_OK, NULL, 0);
	band_print_params();
}

static void cmd_write_encryption_key(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_read_encryption_key(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_write_secret_number(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_read_secret_number(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

static void cmd_activation_timeout(void)
{
	cmd_send_response(blecmd.opcode, CMDST_ERROR, NULL, 0);
}

