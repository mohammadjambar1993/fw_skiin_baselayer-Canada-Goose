/*
 * ble_rpc.h
 *
 *  Created on: Nov 1, 2016
 *      Author: Myant
 */

#ifndef SRC_BLE_RPC_H_
#define SRC_BLE_RPC_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stdint.h>
#include "hal_ble.h"

#define MAX_NTF_ATTEMPTS	10
#define LENCHR_LOGGER		20

typedef enum
{
    BLEMSG_MODINFO  	= 0,
    BLEMSG_COMMAND  	= 1,
    BLEMSG_RESPONSE 	= 2,
	BLEMSG_LOGGER   	= 3,
	BLEMSG_HEATINFO  	= 4,
    //dont define messages below this point
    MAX_BLE_CHARS,
}ble_msgid_e;

bool ble_init(void);
bool ble_connect(void);
bool ble_is_connected(void);
bool ble_is_config_ok(void);
bool ble_is_indication_on(ble_msgid_e msgid);
bool ble_is_notification_on(ble_msgid_e msgid);
void ble_clean_push_queue(void);
void ble_advertise_start(void);
uint16_t ble_get_conn_interval(void);
CharData *ble_get_char(ble_msgid_e msgid);
BLEStatus ble_update_modinfo(bool notify);
BLEStatus ble_update_heatinfo(bool notify);
BLEStatus ble_update_battery_level(uint8_t level);
BLEStatus ble_notify(ble_msgid_e id, void *data, uint8_t len);
BLEStatus ble_indicate(ble_msgid_e id, void *data, uint8_t len);
BLEStatus ble_try_notify(ble_msgid_e id, void *data, uint8_t len,
                         uint8_t attempts);

#if defined(__cplusplus)
}
#endif /* __cplusplus */


#endif /* SRC_BLE_RPC_H_ */
