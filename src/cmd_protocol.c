/*
 * cmd_protocol.c
 *
 *  Created on: Nov 16, 2017
 *      Author: Myant
 */

#include <string.h>
#include "cmd_protocol.h"

#include "ble_rpc.h"
#include "tskctrl.h"
#include "logger.h"
#include "temperature_ads.h"
//#include "hal_pwm.h"

#define LEN_RESP        20  //maximum data length used in response message
#define OPCODE_LENGTH   2   //command opcode length

static uint8_t buffer[LEN_RESP];

cmd_status cmd_get_command(cmdflag_t *cmd)
{
    if(NULL == cmd)
        return CMDST_INV_CMD;
    CharData *chrcmd = ble_get_char(BLEMSG_COMMAND);
    if(chrcmd->validlen < OPCODE_LENGTH)
        return CMDST_INV_OPCODE;

    //process command
    memcpy(&cmd->opcode, chrcmd->data, OPCODE_LENGTH);
    cmd->datalen = chrcmd->validlen-OPCODE_LENGTH;
    cmd->newcmd = true;
    memcpy(&cmd->data, &chrcmd->data[OPCODE_LENGTH], cmd->datalen);

    return CMDST_OK;
}

#define DPOT_ID_SG      0
#define DPOT_ID_GAIN    1
#define IX_DPOTVALUE    0
static cmd_status get_dpot(cmdflag_t *cmd, uint8_t *dpot, uint8_t dpotid)
{
    if((NULL == cmd) || (NULL == dpot))
        return CMDST_INV_CMD;
    if((DPOT_ID_SG == dpotid) && (OPC_SET_DPOT_SG != cmd->opcode))
        return CMDST_INV_DATA;
    if((DPOT_ID_GAIN == dpotid) && (OPC_SET_DPOT_GAIN != cmd->opcode))
        return CMDST_INV_DATA;
    if(cmd->datalen < 1)
        return CMDST_INV_LEN;

    *dpot = cmd->data[IX_DPOTVALUE];
    return CMDST_OK;
}

cmd_status cmd_get_dpot_sg(cmdflag_t *cmd, uint8_t *dpot)
{
   return get_dpot(cmd, dpot, DPOT_ID_SG);
}

cmd_status cmd_get_dpot_gain(cmdflag_t *cmd, uint8_t *dpot)
{
    return get_dpot(cmd, dpot, DPOT_ID_GAIN);
}

cmd_status cmd_get_diagid(cmdflag_t *cmd, diagnostic_id *diag)
{
    if((NULL == cmd) || (NULL == diag))
        return CMDST_INV_CMD;
    if(OPC_DUMP_DIAGNOSTIC != cmd->opcode)
        return CMDST_INV_OPCODE;
    if(cmd->datalen < 1)
        return CMDST_INV_LEN;

    *diag = (diagnostic_id)cmd->data[0];
    return CMDST_OK;
}

cmd_status cmd_get_sens_mode(cmdflag_t *cmd, uint8_t *mode, uint8_t len)
{
    if((NULL == cmd) || (NULL == mode))
        return CMDST_INV_CMD;
    if(OPC_SET_SENS_MODE != cmd->opcode)
        return CMDST_INV_OPCODE;
    if((cmd->data[0]!=SENS_OP_SINGLE_END)&&(cmd->data[0]!=SENS_OP_DIFF_END))
    	return CMDST_INV_DATA;
    if((cmd->datalen != len)||(len!=SENS_MODE_PARAMS))
        return CMDST_INV_LEN;

    memcpy(mode,cmd->data,len);
    return CMDST_OK;
}

cmd_status cmd_get_time6b(cmdflag_t *cmd, uint32_t *msb, uint32_t *lsb)
{
    if((NULL == cmd) || (NULL == msb) || (NULL == lsb))
        return CMDST_INV_CMD;
    if(OPC_SET_RTC != cmd->opcode)
        return CMDST_INV_OPCODE;
    if(cmd->datalen < 6)
        return CMDST_INV_LEN;

    *lsb = cmd->data[0] + (cmd->data[1] << 8) + (cmd->data[2] << 16) +
          (cmd->data[3] << 24);
    *msb = cmd->data[4] + (cmd->data[5] << 8);

    return CMDST_OK;
}

cmd_status cmd_get_ts64(cmdflag_t *cmd, uint64_t *ts)
{
    if((NULL == cmd) || (NULL == ts))
        return CMDST_INV_CMD;
    if(OPC_SET_TS64 != cmd->opcode)
        return CMDST_INV_OPCODE;
    if(cmd->datalen < 8)
        return CMDST_INV_LEN;

    memcpy(ts, cmd->data, sizeof(uint64_t));

    return CMDST_OK;
}

cmd_status cmd_get_activation_timeout(cmdflag_t *cmd, uint16_t *seconds)
{
	if(NULL == cmd || NULL == seconds)
		return CMDST_INV_DATA;
	if(OPC_ACTIVE_TIMEOUT != cmd->opcode)
		return CMDST_INV_OPCODE;
	if(cmd->datalen < 2)
		return CMDST_INV_LEN;

	memcpy(seconds, cmd->data, 2);

	return CMDST_OK;
}

//to get channel PWM parameters or temperature settings
//cmd_status cmd_get_heating_params(cmdflag_t *cmd, uint8_t *para, uint8_t len)
cmd_status cmd_get_heating_params(cmdflag_t *cmd, cmd_heat_params_t *params)
{
	if((NULL == cmd) || (NULL == params))
        return CMDST_INV_CMD;
    if(OPC_SET_HEAT_POWER != cmd->opcode)
        return CMDST_INV_OPCODE;
    if(cmd->datalen != 9)
		return CMDST_INV_LEN;
    //copy duty cycles of all channels
    memcpy(params->data, cmd->data, 5);
    //read period
    params->pwm_period = cmd->data[5];
    //read voltage
    params->voltage = cmd->data[6];
    memcpy(&params->timeout_secs, &cmd->data[7],sizeof(uint16_t));

    if(params->timeout_secs>7200)
    	params->timeout_secs = 7200;

    return CMDST_OK;
}

cmd_status cmd_get_temp_setpoint(cmdflag_t *cmd, cmd_heat_params_t *params)
{
	if((NULL == cmd) || (NULL == params))
		return CMDST_INV_CMD;
	if(OPC_SET_HEAT_TEMP != cmd->opcode)
		return CMDST_INV_OPCODE;
	if(cmd->datalen != 7)
		return CMDST_INV_LEN;
	//copy temperature setpoint of all channels
	memcpy(params->data, cmd->data, 5);
	memcpy(&params->timeout_secs, &cmd->data[5],sizeof(uint16_t));

	if(params->timeout_secs>7200)
	    params->timeout_secs = 7200;
	//period and voltage are not used for this command
	params->pwm_period = 0;
	params->voltage = 0;
	return CMDST_OK;
}

cmd_status cmd_get_ads_config(cmdflag_t *cmd, cmd_ads_config_t *config)
{
	if((NULL == cmd) || (NULL == config))
        return CMDST_INV_CMD;
    if(OPC_WRITE_ADS_REGS != cmd->opcode)
        return CMDST_INV_OPCODE;
    if(cmd->datalen != 13)
		return CMDST_INV_LEN;
    config->gain = cmd->data[0];
    config->PGA = cmd->data[1];
    config->SPS = cmd->data[2];
    config->oprt_mode = cmd->data[3];
    config->conv_mode = cmd->data[4];
    config->temp_sensor_mode = cmd->data[5];
    config->BCSource = cmd->data[6];
    config->Vref = cmd->data[7];
    config->filter_config = cmd->data[8];
    config->power_switch = cmd->data[9];
    config->IDACsetting = cmd->data[10];
    memcpy(&config->Rref, &cmd->data[11],2);

    return CMDST_OK;
}

cmd_status cmd_get_leds(cmdflag_t *cmd, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if((NULL == cmd)||(NULL == r)||(NULL == g)||(NULL == b))
        return CMDST_INV_CMD;
    if(OPC_CTRL_LEDS != cmd->opcode)
        return CMDST_INV_OPCODE;
    if(cmd->datalen < 3)
        return CMDST_INV_LEN;

    *r = cmd->data[0];
    *g = cmd->data[1];
    *b = cmd->data[2];
    return CMDST_OK;
}

void cmd_send_response(cmd_opc opc, cmd_status st, void *bytes, uint8_t len)
{
    if(len >(LEN_RESP-OPCODE_LENGTH-1) )
    {
        log_warn("max cmd response length is 17 bytes!");
        return;
    }
    uint8_t *data = (uint8_t*)bytes;
    uint16_t opcode = (uint16_t)opc;
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &opcode, OPCODE_LENGTH);
    buffer[OPCODE_LENGTH] = (uint8_t)st;
    if(NULL != data)
        memcpy(&buffer[OPCODE_LENGTH+1], data, len);
    else
        len = 0;
    ble_indicate(BLEMSG_RESPONSE, buffer, len+OPCODE_LENGTH+1);
}

cmd_status cmd_get_band_params(cmdflag_t *cmd, band_config_t *bandcfg)
{
	if((NULL == cmd) || (NULL == bandcfg))
		return CMDST_INV_CMD;
	if(OPC_WRITE_BAND_PARAMS != cmd->opcode)
		return CMDST_INV_OPCODE;
	if(cmd->datalen < 3) //id, index and 1 data
		return CMDST_INV_LEN;
	bandcfg->id = cmd->data[0];
	bandcfg->index = cmd->data[1];
	bandcfg->length = cmd->datalen-2;
	memcpy(bandcfg->data, &cmd->data[2], cmd->datalen-2);
	return CMDST_OK;
}

cmd_status cmd_get_serial(cmdflag_t *cmd, uint8_t *serial,
                        uint8_t len_serial)
{
    if((NULL == cmd)||(NULL == serial))
        return CMDST_INV_DATA;
    if(len_serial != 6)
    	return CMDST_INV_LEN;
    if(OPC_WRITE_SERIAL_NUMBER != cmd->opcode)
        return CMDST_INV_OPCODE;
    if(cmd->datalen < 6) //5 bytes serial #
        return CMDST_INV_LEN;
    memcpy(serial, &cmd->data[0], 6);
    return CMDST_OK;
}

cmd_status cmd_get_key(cmdflag_t *cmd, uint8_t *key,
                        uint8_t len_key)
{
    if((NULL == cmd)||(NULL == key)||(len_key != 16))
        return CMDST_INV_CMD;
    if(OPC_WRITE_ENCRYPTION_KEY != cmd->opcode)
        return CMDST_INV_OPCODE;
    if(cmd->datalen < 16) //16 bytes key
        return CMDST_INV_LEN;
    memcpy(key, &cmd->data[0], 16);
    return CMDST_OK;
}

cmd_status cmd_get_secret(cmdflag_t *cmd, uint8_t *secret,
                        uint8_t len)
{
    if((NULL == cmd)||(NULL == secret)||(len != 16))
        return CMDST_INV_CMD;
    if(OPC_WRITE_SECRET_NUMBER != cmd->opcode)
        return CMDST_INV_OPCODE;
    if(cmd->datalen < 16) //16 bytes key
        return CMDST_INV_LEN;
    memcpy(secret, &cmd->data[0], 16);
    return CMDST_OK;
}
