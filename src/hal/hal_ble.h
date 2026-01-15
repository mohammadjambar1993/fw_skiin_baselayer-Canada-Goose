/*
 * hal_ble.h
 *
 *  Created on: Oct 3, 2016
 *      Author: Myant
 */

#ifndef SRC_HAL_HAL_BLE_H_
#define SRC_HAL_HAL_BLE_H_

#include "hal_config.h"
#include "ble.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* MAX_SUPPORTED_CHARS: maximum supported characteristics. If this # needs to
 * be increased, probably the attribute table size will need to be increased
 * too as well as the linker file! */
#define MAX_SUPPORTED_CHARS 13

#define UUID_CCCD           0x2902
#define NTF_ENABLED         0x0001
#define IND_ENABLED         0x0002
//maximum length for characteristics data
#define MAX_LEN_CHAR   		(MYANT_BLE_MTU-3)

typedef enum
{
    BLEST_OP_FAIL       = 0,
    BLEST_OP_OK         = 1,
    BLEST_CALLB_FULL    = 2,
    BLEST_CALLB_INVALID = 3,
    BLEST_HND_INVALID   = 4,
    BLEST_LEN_INVALID   = 5,
    BLEST_NOT_ENABLED   = 6,
    BLEST_NOT_CONNECTED = 7,
    BLEST_QUEUE_FULL    = 8,
    BLEST_NO_TX_BUFFERS = 9,
    BLEST_CHARS_FULL    = 10,
    BLEST_INVALID_ID    = 11,
    BLEST_INVALID_PARAMS = 12,
}BLEStatus;

typedef enum
{
    BLEEVT_COMMON,
    BLEEVT_CONNECTION,
    BLEEVT_CONNECTION_ERROR,
    BLEEVT_ADVERTISE,
    BLEEVT_SYSTEM,
}BLEEvent;

typedef enum
{
    BLEMODE_WRITE =     1,
    BLEMODE_READ =      2,
    BLEMODE_NOTIFY =    4,
    BLEMODE_INDICATE =  8
}CharMode;

typedef struct
{
    uint8_t uuid128[16];
    uint16_t uuid16;
}ServiceData;

typedef struct
{
    uint8_t uuid128[16];
    uint16_t uuid;
    uint16_t hnd_service;
    uint16_t handle;
    uint16_t len;
    uint8_t *data;      //pointer to buffer that holds the data
    uint8_t validlen;   //how many data bytes are valid
    uint8_t mode;
}CharData;

BLEStatus hal_ble_init(char *name);
uint16_t hal_ble_get_mtu(void);
BLEStatus hal_ble_init_sd(void);
BLEStatus hal_ble_deinit_sd(void);
BLEStatus hal_ble_connect(void);
BLEStatus hal_ble_disconnect(void);
BLEStatus hal_ble_set_module_name(const char *name);
BLEStatus hal_ble_add_battery_srv(void);
BLEStatus hal_ble_set_battery_level(uint8_t level);
BLEStatus hal_ble_set_advdata(uint8_t *data, uint8_t length);
BLEStatus hal_ble_advertise_start(const uint16_t *uuids, uint16_t len);
BLEStatus hal_ble_advertise_no_connect(void);
BLEStatus hal_ble_advertise_restart(void);
BLEStatus hal_ble_advertise_stop(void);
BLEStatus hal_ble_add_service(const ServiceData *service, uint16_t *handle);
BLEStatus hal_ble_add_characteristic(const CharData *charac, void *init_value,
                                     uint16_t *handle);
BLEStatus hal_ble_read_char(uint16_t handle, void *data, uint16_t *len);
BLEStatus hal_ble_update_chr(uint16_t hnd, void *data, uint16_t len);
BLEStatus hal_ble_update_cccd(uint16_t hnd, uint16_t value);
BLEStatus hal_ble_reset_all_cccd(void);
BLEStatus hal_ble_push(CharMode mode, uint16_t hnd, void *data, uint16_t len);
BLEStatus hal_ble_add_evt_listener(void(*l)(BLEEvent id, void *data,
                                            uint16_t len));
BLEStatus hal_ble_get_addr(uint8_t *addr, uint8_t len);
BLEStatus hal_ble_get_name(uint8_t *name, uint16_t *len);
BLEStatus hal_ble_get_conn_interval(uint16_t *max, uint16_t *min);
BLEStatus hal_ble_get_max_throughput(uint16_t conn_interval, float *bytesps);
BLEStatus hal_ble_get_char_handle(uint16_t uuid, uint16_t *hnd);
uint16_t hal_ble_get_conn_handle(void);
uint16_t hal_ble_get_cccd(uint16_t handle);
bool hal_ble_is_push_on_by_char_hnd(uint16_t hnd);
int8_t hal_ble_get_txpower(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_HAL_HAL_BLE_H_ */
