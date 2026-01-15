/*
 * cmd_uicr.h
 *
 *  Created on: May 16, 2019
 *      Author: Ela Asgari
 */

#ifndef SRC_CMD_UICR_H_
#define SRC_CMD_UICR_H_

#include <stdbool.h>
#include <stdint.h>

#include "diagnostic.h"
#include "hal_ble.h"

typedef enum
{
    UICRST_OK           = 0,
	UICRST_FAIL   		= 1,
	UICRST_INV_ADD      = 2,
	UICRST_INV_DATA     = 3,
	UICRST_INV_LENGTH   = 4,
	UICRST_NOT_ERASED   = 5,
}uicr_status;

typedef enum
{
    UICR_SRC_KEY            = 0,
    UICR_SRC_SERIAL_HIGH    = 1,
    UICR_SRC_SERIAL_LOW     = 2,
    UICR_SRC_ENCRYPT_KEY3   = 3,
	UICR_SRC_ENCRYPT_KEY2   = 4,
	UICR_SRC_ENCRYPT_KEY1   = 5,
	UICR_SRC_ENCRYPT_KEY0   = 6,
    UICR_SRC_SECRET_NUM3    = 7,
    UICR_SRC_SECRET_NUM2    = 8,
	UICR_SRC_SECRET_NUM1    = 9,
	UICR_SRC_SECRET_NUM0    = 10,
	UICR_SRC_APP_PROTECT    = 12,
    UICR_SRC_NFC_PINS       = 13,
}uicr_src_t;


uicr_status uicr_read(uicr_src_t src, uint32_t *data);
uicr_status uicr_write(uicr_src_t src, uint32_t value);
uicr_status uicr_disable_debugger(void);
uicr_status uicr_set_nfc_gpio(void);
uicr_status uicr_read_serial_number(uint8_t *serial, uint8_t len);
uicr_status uicr_write_serial_number(uint8_t *serial, uint8_t len);
uicr_status uicr_read_encryption_key(uint8_t *key, uint8_t len);
uicr_status uicr_write_encryption_key(uint8_t *key, uint8_t len);
uicr_status uicr_read_secret_number(uint8_t *secret, uint8_t len);
uicr_status uicr_write_secret_number(uint8_t *secret, uint8_t len);

#if defined(__cplusplus)
}
#endif /* __cplusplus */
#endif /* SRC_CMD_UICR_H_ */
