/*
 * ble_protocol.h
 * Module that provides functions to format commands received from BLE
 *  Created on: Nov 16, 2017
 *      Author: Myant
 */

#ifndef SRC_CMD_PROTOCOL_H_
#define SRC_CMD_PROTOCOL_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stdint.h>
#include "hal_ble.h"
#include "diagnostic.h"
#include "sensors.h"
#include "heater.h"

/*
 * refer to BLE protocol
 */
#define LEN_TS_FROM_MOBILE         6 //# bytes received from phone to set RTC

typedef enum
{
    OPC_SET_SENS_MODE   = 0x0002,  //set resistance measurement mode and params
    OPC_RESET_TS        = 0x0003,  //reset timestamp
    OPC_SET_DPOT_SG     = 0x0004,  //set strain gauge potentiometer
    OPC_RESET_MAX       = 0x0005,  //reset MAX30001
    OPC_READ_INFO_MAX   = 0x0006,  //read MAX30001 info register
    OPC_DUMP_DIAGNOSTIC = 0x0007,  //send diagnostic info in log message
    OPC_SET_DPOT_GAIN   = 0x0008,  //set strain gauge gain potentiometer
    OPC_START_DFU       = 0x0009,  //start OTA service
	OPC_SET_RTC			= 0x000A,  //set real-time clock (RTC) 64-bit timestamp
	/** opcode 0x000B was just used for Energous wattup mode is deprecated.
	 * It must NOT be used for new commands */
	OPC_SET_OPMODE      = 0x000C,  //set operation mode
	OPC_RESET_MODULE	= 0x000D,  //reset embedded module
	OPC_ENTER_SHIPMODE  = 0x000E,  //enter ship mode to save battery
	OPC_ENTER_SIMUMODE  = 0x000F,  //all the sensors to simulation mode
	OPC_ERASE_FLASH     = 0x0010,  //erase metrics in flash memory
	OPC_ACTIVE_TIMEOUT	= 0x001E,  //set heat timeout after BLE disconnection
	OPC_WRITE_ADS_REGS	= 0x001F,  //set ADS registers (Write operation)
	OPC_SET_TS64        = 0x0011,  //test command to set 64bit ts
	OPC_GEN_HF          = 0x0012,  //command to generate a hardfault reset
	OPC_GET_TS64        = 0x0013,  //command to read back TS64
	OPC_READ_ADS_REGS	= 0x0014,  //command to read ADS registers
	OPC_SET_HEAT_POWER  = 0x0017,  //command to set PWM on heat channel
	OPC_CTRL_LEDS       = 0x0018,
	OPC_SET_HEAT_TEMP   = 0x0019, //command to set temperature
	OPC_WRITE_ENCRYPTION_KEY	= 0x0031,
	OPC_READ_ENCRYPTION_KEY		= 0x0032,
	OPC_WRITE_SECRET_NUMBER		= 0x0033,
	OPC_READ_SECRET_NUMBER		= 0x0034,
	OPC_WRITE_SERIAL_NUMBER		= 0x0035,  //get 5 bytes of unique serial number
	OPC_READ_SERIAL_NUMBER		= 0x0036,  //read serial number
	OPC_PRINT_BAND_PARAMS		= 0x0020,  //print temperature control parameters
	OPC_WRITE_BAND_PARAMS		= 0x0021,  //write band parameters in RAM buffer
	OPC_READ_BAND_PARAMS		= 0x0022,  //read band parameters from RAM buffer
	OPC_PERSIST_BAND_PARAMS		= 0x0023,  //write band parameters in flash
	OPC_MAX             = 0xFFFF,   //add this value to make sure it is int16
}cmd_opc;

typedef enum
{
    CMDST_OK            = 0,
    CMDST_INV_OPCODE    = 1,
    CMDST_INV_LEN       = 2,
    CMDST_INV_CMD       = 3,
    CMDST_INV_DATA      = 4,
    CMDST_ERROR         = 5,
	CMDST_INV_STATE     = 6,
}cmd_status;

typedef struct
{
    uint16_t opcode;
    void(*parser)(void);
}command_t;

typedef struct
{
    bool newcmd;
    cmd_opc opcode;
    uint16_t datalen;
    uint8_t data[MAX_LEN_CHAR];
}cmdflag_t;

typedef struct
{
	heater_id_t id;
	uint8_t index;
	uint8_t length;
	uint8_t data[16];
}band_config_t;

typedef struct
{
	/* data can be duty cycle or temperature
	 * setpoint values depending on the command */
	uint8_t data[MAX_HEATERS];
	uint8_t pwm_period;
	uint8_t voltage;
	uint16_t timeout_secs;
}cmd_heat_params_t;

typedef struct
{
	uint8_t gain;
	uint8_t PGA;
	uint8_t SPS;
	uint8_t oprt_mode;
	uint8_t conv_mode;
	uint8_t temp_sensor_mode;
	uint8_t BCSource;
	uint8_t Vref;
	uint8_t filter_config;
	uint8_t power_switch;
	uint8_t IDACsetting;
	uint16_t Rref;
}__attribute__((packed, aligned(1)))cmd_ads_config_t;

cmd_status cmd_get_command(cmdflag_t *cmd);
cmd_status cmd_get_dpot_sg(cmdflag_t *cmd, uint8_t *dpot);
cmd_status cmd_get_dpot_gain(cmdflag_t *cmd, uint8_t *dpot);
cmd_status cmd_get_diagid(cmdflag_t *cmd, diagnostic_id *diag);
cmd_status cmd_get_sens_mode(cmdflag_t *cmd, uint8_t *mode, uint8_t len);
cmd_status cmd_get_time6b(cmdflag_t *cmd, uint32_t *msb, uint32_t *lsb);
cmd_status cmd_get_ts64(cmdflag_t *cmd, uint64_t *ts);
cmd_status cmd_get_activation_timeout(cmdflag_t *cmd, uint16_t *seconds);
cmd_status cmd_get_heating_params(cmdflag_t *cmd, cmd_heat_params_t *params);
cmd_status cmd_get_temp_setpoint(cmdflag_t *cmd, cmd_heat_params_t *params);
void cmd_send_response(cmd_opc opc, cmd_status st, void *bytes, uint8_t len);
cmd_status cmd_get_leds(cmdflag_t *cmd, uint8_t *r, uint8_t *g, uint8_t *b);
cmd_status cmd_get_band_params(cmdflag_t *cmd, band_config_t *bandcfg);
cmd_status cmd_get_serial(cmdflag_t *cmd, uint8_t *serial,
                        uint8_t len_serial);
cmd_status cmd_get_key(cmdflag_t *cmd, uint8_t *key,
                        uint8_t len_key);
cmd_status cmd_get_secret(cmdflag_t *cmd, uint8_t *secret,
                        uint8_t len);
cmd_status cmd_get_ads_config(cmdflag_t *cmd, cmd_ads_config_t *config);

#if defined(__cplusplus)
}
#endif /* __cplusplus */
#endif /* SRC_CMD_PROTOCOL_H_ */
