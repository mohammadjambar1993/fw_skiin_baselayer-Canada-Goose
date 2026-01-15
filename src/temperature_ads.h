/*
 * temperature_ads.h
 *
 *  Created on: Sep 16, 2020
 *      Author: Jeffrey Zhu
 */

#ifndef SRC_TEMPERATURE_ADS_H_
#define SRC_TEMPERATURE_ADS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "apptypes.h"
#include "cmd_protocol.h"
#include "heater.h"
#include "appconfig.h"

#define SENS_OP_SINGLE_END       1  //2-Wire RTD in ADS1220
#define SENS_OP_DIFF_END         2  // 4-Wire Differantial mode in ADS1220
#define SENS_MODE_PARAMS         7
#define SENS_DEFAULT_PARAMS      5
#define SEND_BUFFER_MAX	  		 3

bool ads_init(void);
int16_t ads_get_resistance(void);
uint8_t ads_temperature_get_mode(void);
int16_t ads_get_PCB_temperature(void);
app_status_t ads_write_config(cmd_ads_config_t *config);
app_status_t ads_read_config(cmd_ads_config_t *config_r);

#endif /* SRC_TEMPERATURE_ADS_H_ */
