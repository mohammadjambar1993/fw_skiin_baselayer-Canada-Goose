/*
 * ina231.h
 *
 *  Created on: Aug 1, 2019
 *      Author: Myant
 */

#ifndef SRC_DRIVERS_INA231_H_
#define SRC_DRIVERS_INA231_H_

#include <stddef.h>
#include <stdbool.h>
#include "hal_config.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
	CUR_ST_OK,
	CUR_ST_FAIL
}cur_status;

typedef enum
{
	CUR_CH1=0,
	CUR_CH2,
#if PCB_ID == PCB_MULTI_CHANNEL
	CUR_CH3,
	CUR_CH4,
	CUR_CH5,
#endif
	CUR_CH_NUM,
}cur_channel_t;

// 9ms needed to finish conversion for current sensor
#define CUR_WAITING_TIME  10
bool cur_sensor_init(uint8_t ch, uint16_t shunt_res, uint16_t cur_limit);
/*before read data from sensors,need to command sensors to start AD conversion
 *it take about 9ms to finish the conversion */
cur_status cur_start_conversion(cur_channel_t ch);
/*the readback current is in mA */
cur_status cur_read_current(cur_channel_t ch, int16_t *data);
//the readback current is in mV
cur_status cur_read_voltage(cur_channel_t ch, uint16_t *data);
//check alert status, set short-circuit status
cur_status cur_alert_status(cur_channel_t ch, uint8_t * short_status);
#if defined(__cplusplus)
}
#endif /* __cplusplus */


#endif /* SRC_DRIVERS_INA231_H_ */
