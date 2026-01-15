/*
 * ble_evt.h
 *
 *  Created on: Dec 6, 2016
 *      Author: Myant
 */

#ifndef SRC_BLE_EVT_H_
#define SRC_BLE_EVT_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    EVT_CONNECTED = 0,
    EVT_DISCONNECTED,
    EVT_CONN_UPDATE,
    EVT_HANDLE_WRITE,
    EVT_NOTIFICATION_TX,
    EVT_INDICATION_TX,
    EVT_LONG_WRITE,
}evt_id_t;

typedef struct
{
    uint16_t handle;
    uint16_t uuid;
    uint16_t len_data;
    uint8_t *data;
}bleevt_callb_data_t;

typedef enum
{
    FMTST_FAIL          = 0,
    FMTST_OK            = 1,
    FMTST_UNKNOWN_EVT   = 2,
    FMTST_NULL_PTR      = 3,
    FMTST_BAD_LENGTH    = 4,
}fmt_st_e;

typedef void(*bleevt_callb_t)(evt_id_t id, void *data, uint16_t len);

void ble_evt_init(void);
void ble_evt_add_calback(bleevt_callb_t callb);
fmt_st_e ble_evt_format_data(evt_id_t id, void *fmt, void *data, uint16_t len);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_BLE_EVT_H_ */
