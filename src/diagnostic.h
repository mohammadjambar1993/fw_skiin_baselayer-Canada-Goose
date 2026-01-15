/*
 * diagnostic.h
 *
 *  Created on: Oct 31, 2017
 *      Author: Myant
 */

#ifndef SRC_DIAGNOSTIC_H_
#define SRC_DIAGNOSTIC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "tskctrl.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#define HARDFAULT_TAG   0x11223344
#define MAX_MODULE_NUM  999999999

typedef enum
{
    DIAG_STACK      = (1<<0),
    DIAG_STACK_ALL  = (1<<1),
    DIAG_ERRORS     = (1<<2),
    DIAG_ERRORS_ALL = (1<<3),
}diagnostic_id;

typedef enum
{
    //BLE flags
    FLG_BLE_EVT_LEN             = 0,
    FLG_BLE_MAX_EVENTS,
    FLG_BLE_EVT_QUEUE_FULL,
    FLG_BLE_PUSHQ_FULL,
    FLG_BLE_PUSH_ENQUEUE,
    FLG_BLE_PUSHTSK_ERROR,
    FLG_BLE_ADV_ERROR,
    FLG_BLE_ADV_NO_CONNECT,
    FLG_BLE_ADV_RESTART,
    FLG_BLE_ADV_STOP,
    FLG_BLE_CMD_OVF,
    FLG_BLE_FMT_NULL,
    FLG_BLE_FMT_LENGTH,
    FLG_BLE_INIT_HAL,
    FLG_BLE_UPDATE_CONN,
    FLG_BLE_UPDATE_PHY,
    FLG_BLE_CMD_OVERFLOW,
    FLG_BLE_UPDATE_REQUEST,
    FLG_BLE_UPDATE_FAILED,
    FLG_BLE_SET_ADV_DATA,
	FLG_BLE_PM_SET,
	FLG_BLE_PM_REG,
	FLG_BLE_PM_SEC_FAILED,
	FLG_BLE_PM_SEC_LINK,
	FLG_BLE_GATT_INIT,
	FLG_BLE_GATT_SERVICE,
	FLG_BLE_GAP_NOCONN,
	FLG_BLE_SET_NAME,
    //gtk spi
    FLG_GTKSPI_ENQUEUE_ERROR,
    FLG_GTKSPI_TXTIMEOUT,
    FLG_GTKSPI_DEQUEUE_TX,
    FLG_GTKSPI_TX_SPACE,
    //max30001 (ECG)
    FLG_BIO_SPI_TIMEOUT,
    FLG_BIO_SPI_GTKERROR,
    //bmi160 (IMU)
    FLG_BMI_SPI_TIMEOUT,
    FLG_BMI_SPI_GTKERROR,
    FLG_BMI_READ_ERROR,
    FLG_BMI_INIT_ERROR,
    FLG_BMI_CONFIG_ERROR,
    FLG_BMI_FIFO_CFG_ERROR,
    FLG_BMI_STEPS_CONFIG,
    FLG_BMI_STEPS_READ,
    FLG_BMI_SEPS_RESET,
	FLG_BMI_CONFIG_ANY_MOTION,
	FLG_BMI_READ_ANY_MOTION,
	//AD1220
	FLG_AD_SPI_TIMEOUT,
	FLG_AD_SPI_TXERROR,
	//sensors
	FLG_ACC_DEQUEUE,
	FLG_IMU_INIT,
    //external flash memory
	FLG_MEM_INIT,
    FLG_MEM_SPI_TIMEOUT,
    FLG_MEM_SPI_TXERROR,
	FLG_MEM_SPI_GTKERROR,
    //i2c port
    FLG_I2C_IRQ_PMIC,
	FLG_I2C_IRQ_INA,  //current sensor
    //pmic
    FLG_PMIC_INIT,
	FLG_PMIC_BATT_RANGE,
    FLG_PMIC_READ_DATA,
	FLG_PMIC_READ,
	FLG_PMIC_WRITE,
	//ina231
	FLG_INA_READ,
	FLG_INA_WRITE,
	//Temperature module
    FLG_HF_MISSED_IRQ,
	FLG_TEMP_BLE_NOTIFY,
	//adc inside nRF
	FLG_ADC_IRQ,
	//watchdog timer
    FLG_WDT_INIT,
    FLG_WDT_ALLOC,
    FLG_WDT_CONFIG,
    FLG_WDT_TIMEOUT,
    //activity flags
    FLG_ACT_STEPS_MISSED,
    FLG_ACT_BLE_NOTIFY,
    //uicr registers flags
    FLG_UICR_READ_SERIAL,
	FLG_UICR_WRITE_SERIAL,
    FLG_MTU_TIMER_CREATE,
    FLG_MTU_TIMER_START,
    FLG_MTU_EXCHANGE,
	//pwm
	FLG_PWM_INIT,
    //ble command
    FLG_COMMAND_INVALID,
	// Heat_ctrl
	FLG_HEAT_TIMER_CREATE,
	FLG_HEAT_TIMER_START,
    //do not declare new flags below MAX_FLAGS!
    MAX_FLAGS
}flag_id;

typedef enum
{
    SYSEVT_MAXCT        = 0,
    SYSEVT_WDT_TIMEOUT  = 1,
}sys_evt_id;

typedef enum
{
    RSTSRC_HARDW    = (1<<0),   //hardware reset (rst pin or power on)
    RSTSRC_WDT      = (1<<1),
    RSTSRC_SWRST    = (1<<2),
    RSTSRC_LOCKUP   = (1<<3),
    RSTSRC_SYSOFF   = (1<<4),
    RSTSRC_LPCOMP   = (1<<5),
    RSTSRC_DBGIF    = (1<<6),
    RSTSRC_NFC      = (1<<7),
    RSTSRC_HARDF    = (1<<8),
}reset_src;

void diag_init(void);
uint16_t diag_reset_flag(flag_id id);
uint16_t diag_inc_flag(flag_id id);
uint16_t diag_dec_flag(flag_id id);
uint16_t diag_get_rst_cause(void);
bool diag_is_rst(reset_src src);
void diag_flag_wdt_timeout(TaskId id);
void diag_dump(diagnostic_id diags);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_DIAGNOSTIC_H_ */
