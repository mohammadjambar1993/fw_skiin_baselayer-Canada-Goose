/*
 * sensors.h
 *
 *  Created on: Sep 28, 2017
 *      Author: Myant
 */

#ifndef SRC_SENSORS_H_
#define SRC_SENSORS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    SENS_OK             = 0,
    SENS_ERROR          = 1,
    SENS_INV_STATE      = 2,
    SENS_INV_CONFIG     = 3,
}sens_status_e;

typedef enum
{
    SENSST_SUSPENDED    = 3,
    SENSST_IDLE         = 4,
    SENSST_RUNNING      = 5,
    SENSST_BOOTING      = 6,
}sens_state_e;

typedef enum
{
    IC_PMIC         = 0,
    IC_IMU          = 1,
    IC_ECG1         = 2,
    IC_ECG2         = 3,
    IC_MEMORY       = 4,
    IC_TEMPERATURE  = 5,
}circuit_id_e;

typedef enum
{
    SENS_OP_REAL            = 0,
	SENS_OP_CONSTANT        = 1,
	SENS_OP_COUNTER         = 2,
    SENS_OP_BLE_DROP_TEST   = 3,
    SENS_OP_SIMULATION      = 4,
	SENS_MODE_REAL          = 7,
	SENS_MODE_COUNTER       = 8,
	SENS_MODE_SIMULATION    = 9,
}sens_op_mode_e;

static inline bool sens_is_valid_mode(sens_op_mode_e mode)
{
    return (mode==SENS_OP_REAL ||
            mode==SENS_OP_CONSTANT ||
            mode==SENS_OP_COUNTER ||
            mode==SENS_OP_BLE_DROP_TEST);
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_SENSORS_H_ */
