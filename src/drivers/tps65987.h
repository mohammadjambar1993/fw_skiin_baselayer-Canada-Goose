/*
 * tps65987.h
 *
 *  Created on: Dec 19, 2019
 *      Author: Myant
 */

#ifndef HAL_TPS_65987_H_
#define HAL_TPS_65987_H_

#include "hal_config.h"
#include "../appconfig.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    CHG_READY           = 0,
    CHG_CHARGING        = 1,
    CHG_CHARGED         = 2,
    CHG_FAULT           = 3,
    CHG_DISCONNECTED    = 4,
}charging_status_t;

typedef enum
{
    PMICST_OK           = 0,
    PMICST_FAIL         = 1,
    PMICST_I2C_ERROR    = 2,
    PMICST_OUT_RANGE    = 3
}pmic_status_t;

typedef enum
{
    CHARGE_LIM_512mA     = 8,
    CHARGE_LIM_1024mA    = 16,
	CHARGE_LIM_1536mA    = 24,
    CHARGE_LIM_2048mA    = 32,
}pmic_ilimit;

#if PCB_ID == PCB_MULTI_CHANNEL
	#define DEFAULT_VOLTAGE  90
#elif PCB_ID == PCB_DUAL_CHANNEL
	#define DEFAULT_VOLTAGE  120
#endif

pmic_status_t tps_init(void);
pmic_status_t tps_neg_contract(uint8_t level);
bool tps_voltagelevel_valid(uint8_t level);
uint8_t tps_retrieve_voltagelevel(uint8_t sel);
uint8_t tps_retrieve_max_volt(void);
bool tps_get_available_voltages(uint8_t *voltages, uint8_t *n_voltages);
pmic_status_t tps_read_batt_level(uint16_t * level);  //unit in mv
pmic_status_t tps_read_batt_range(uint8_t *  range);  //unit in percentage


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif //HAL_TPS_65987_H_
