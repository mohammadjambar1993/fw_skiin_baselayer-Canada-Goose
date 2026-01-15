/*
 * ble_evt.c
 *
 *  Created on: Dec 6, 2016
 *      Author: Myant
 */

#include <string.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_gap.h"
#include "ble_conn_params.h"
#include "ble_advertising.h"
#include "ble_evt.h"
#include "hal_config.h"
#include "logger.h"
#include "logio.h"
#include "hal_ble.h"
#include "FreeRTOS.h"
#include "appconfig.h"
#include "diagnostic.h"
#include "task.h"
#include "tskctrl.h"
#include "nrf_sdm.h"
#include "wdt.h"

#define LEN_QUEUE       25
#define MAX_EVT_LEN     sizeof(ble_evt_t)

typedef struct
{
    uint32_t time;                  //timestamp
    BLEEvent id;                    //event identification
    uint8_t len;                    //length of event structure
    uint8_t data[MAX_EVT_LEN];      //array to hold event data
}EventItem;

//buffer to store events from BLE module
static bleevt_callb_t listener = NULL;
static QueueHandle_t evt_queue = NULL;
static wdt_entry wdt = NULL;
static TimerHandle_t timer_mtu = NULL;

static void irq_timer_mtu(TimerHandle_t xtimer);
static inline void create_mtu_timer(void);
static void ble_irq(BLEEvent id, void *data, uint16_t len);
static void tsk_ble_events(void *params);
static void dispatch_event(EventItem *e);
static inline void print_connection_details(ble_gap_conn_params_t *params);
static inline void call_listener(evt_id_t id, void *data, uint16_t len);
static inline void print_char(uint16_t handle, uint8_t *data, uint16_t len)
__attribute__ ((unused));

void ble_evt_init(void)
{
    TaskHandle_t hnd;
    wdt_config_t wdtconf;

    if(BLEST_OP_OK == hal_ble_add_evt_listener(ble_irq))
    {
        hnd = tsk_create(TSK_LIB_BLE_EVENTS, tsk_ble_events);
        ASSERT_TASK(hnd);
        evt_queue = os_create_queue(LEN_QUEUE, sizeof(EventItem));
        ASSERT_QUEUE(evt_queue);
        //configure watchdog timer entry
        wdtconf.enabled = true;
        wdtconf.event_mask = 0;
        wdtconf.id = TSK_LIB_BLE_EVENTS;
        wdtconf.timeout = 2;
        wdtconf.type = WDT_E_BLOCKING;
        wdt = wdt_config_entry(&wdtconf, pdMS_TO_TICKS(200));
    }
}

void ble_evt_add_calback(bleevt_callb_t callb)
{
    listener = callb;
}

void ble_evt_flush_events(void)
{
    uint8_t ptr[4];
    uint16_t l;
    //clean soft device events
    uint32_t status = sd_ble_evt_get(ptr, &l);
    while(status != NRF_ERROR_NOT_FOUND)
        status = sd_ble_evt_get(ptr, &l);
}

static inline void print_connection_details(ble_gap_conn_params_t *params)
{
    float bytesps;
    BLEStatus status;
    uint16_t interval = params->max_conn_interval;
    log_debug("Connection interval: %d = (%f ms)", interval, interval*1.25);
    log_debug("Supervision timeout: %d", params->conn_sup_timeout);
    log_debug("Slave latency: %d", params->slave_latency);
    //get throughput
    status = hal_ble_get_max_throughput(interval, &bytesps);
    if(BLEST_OP_OK == status)
        log_debug("Max expected throughput: %.1f bytes/s", bytesps);
    else
        log_error("Throughput calculation error");
}

static inline void print_char(uint16_t handle, uint8_t *data, uint16_t len)
{
    uint16_t i, k = 0;
    uint32_t integer = 0;

    log_debug("--- handle: 0x%04X, len: %d", handle, len);
    while(len)
    {
        i = (len > 4) ? 4 : len;
        memcpy(&integer, data, i);
        data += 4;
        if(len >= i)
            len -= i;
        else
            len = 0;
        log_debug("--- data[%d]: 0x%08X", k, integer);
        k++;
    }
}

static inline void call_listener(evt_id_t id, void *data, uint16_t len)
{
    if(listener)
        listener(id, data, len);
}

fmt_st_e ble_evt_format_data(evt_id_t id, void *fmt, void *data, uint16_t len)
{
    fmt_st_e status = FMTST_FAIL;
    if(fmt == NULL || data == NULL)
    {
    	diag_inc_flag(FLG_BLE_FMT_NULL);
    	log_error("[FMT] null ptr received on event id: %d", id);
        return FMTST_NULL_PTR;
    }
    if(EVT_HANDLE_WRITE == id)
    {
        if(len < 6) //6: 2b handle, 2b uuid, 2b data length
        {
        	diag_inc_flag(FLG_BLE_FMT_LENGTH);
        	log_error("Wrong length for event: %d", id);
            return FMTST_BAD_LENGTH;
        }
        uint8_t *rawdata = (uint8_t*)data;
        bleevt_callb_data_t *pack = (bleevt_callb_data_t*)(fmt);
        //copy handle
        memcpy(&pack->handle, &rawdata[0], sizeof(pack->handle));
        //copy uuid
        memcpy(&pack->uuid, &rawdata[2], sizeof(pack->uuid));
        //copy data length
        memcpy(&pack->len_data, &rawdata[4], sizeof(pack->len_data));
        //copy data pointer
        pack->data = (len == 6) ? NULL : &rawdata[6];
        status = FMTST_OK;
    }
    else
        status = FMTST_UNKNOWN_EVT;
    return status;
}

static void irq_timer_mtu(TimerHandle_t xtimer)
{
    uint32_t err;
    uint16_t mtu = hal_ble_get_mtu();
    log_info("Current MTU: %d", mtu);
    if(mtu < NRF_SDH_BLE_GATT_MAX_MTU_SIZE)
    {
        log_info("Requesting MTU exchange...");
        err = sd_ble_gattc_exchange_mtu_request(hal_ble_get_conn_handle(),
                                                NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
        if(err != NRF_SUCCESS)
        {
            diag_inc_flag(FLG_MTU_EXCHANGE);
            log_error("Fail exchange mtu: %d", err);
        }
    }
}

static inline void create_mtu_timer(void)
{
    //create timer to check the MTU size
	if(NULL == timer_mtu)
	{
		//create one shot timer
		timer_mtu = xTimerCreate("tmtu",pdMS_TO_TICKS(500),pdFALSE,0,
								 irq_timer_mtu);
		if(NULL == timer_mtu)
		{
			diag_inc_flag(FLG_MTU_TIMER_CREATE);
			log_warn("Failed to create MTU timer");
			return;
		}
	}
    if(pdPASS != xTimerStart(timer_mtu, 0))
    {
    	diag_inc_flag(FLG_MTU_TIMER_START);
    	log_warn("Failed to initialize MTU timer");
    }
}

#define LEN_EVT_DATA    26
static void dispatch_event(EventItem *e)
{
    if(BLEEVT_COMMON == e->id)
    {
        ble_evt_t event;
        memcpy(&event, e->data, e->len);
        if(BLE_GAP_EVT_CONN_PARAM_UPDATE == event.header.evt_id)
        {
            ble_gap_evt_conn_param_update_t params;
            log_debug("[%ld]BLE gap: connection params updated", e->time);
            params = event.evt.gap_evt.params.conn_param_update;
            uint16_t time = params.conn_params.max_conn_interval;
            print_connection_details(&params.conn_params);
            call_listener(EVT_CONN_UPDATE, &time, sizeof(time));
        }
        else if(BLE_GAP_EVT_CONNECTED == event.header.evt_id)
        {
            create_mtu_timer();
            sd_ble_gatts_service_changed(event.evt.gap_evt.conn_handle, 0x0000,
                                                     0xffff);

            ble_gap_evt_connected_t params;
            log_debug("[%ld]BLE gap: connected, handle: %d", e->time,
                    event.evt.gap_evt.conn_handle);
            params = event.evt.gap_evt.params.connected;
            uint16_t time = params.conn_params.max_conn_interval;
            print_connection_details(&params.conn_params);
            call_listener(EVT_CONNECTED, &time, sizeof(time));

        }
        else if(BLE_GAP_EVT_DISCONNECTED == event.header.evt_id)
        {
            xQueueReset(evt_queue);
            wdt_feed(wdt);
            call_listener(EVT_DISCONNECTED, NULL, 0);
            log_debug("[%ld]BLE gap: disconnected", e->time);
        }
        else if(BLE_GAP_EVT_PHY_UPDATE_REQUEST == event.header.evt_id)
	    {
		   ret_code_t err;
		   ble_gap_phys_t const phys =
		   {
			   .rx_phys = BLE_GAP_PHY_CODED,
			   .tx_phys = BLE_GAP_PHY_CODED,
		   };
		   err = sd_ble_gap_phy_update(event.evt.gap_evt.conn_handle, &phys);
		   if(NRF_SUCCESS != err)
		   {
			   diag_inc_flag(FLG_BLE_UPDATE_PHY);
			   log_error("[ble_evt] phy update req: %d", err);
		   }
		   else
			   log_debug("[ble_evt] phy update ok");
	    }
        else if(BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST == event.header.evt_id)
        {
            log_debug("BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST");
            /* code below is left here just for future reference in case we need
             * to deal with exchange mtu request.
               err_code = sd_ble_gatts_exchange_mtu_reply(
                                            event.evt.gatts_evt.conn_handle,
                                            NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
            if(NRF_SUCCESS != err_code)
                log_error("MTU exchange: %d", err_code); */
        }
        else if(BLE_GATTC_EVT_EXCHANGE_MTU_RSP == event.header.evt_id)
        {
            log_debug("BLE_GATTC_EVT_EXCHANGE_MTU_RSP");
            log_info("MTU resp: %d",
                    event.evt.gattc_evt.params.exchange_mtu_rsp.server_rx_mtu);
        }
        else if(BLE_GAP_EVT_DATA_LENGTH_UPDATE == event.header.evt_id)
        {
        	log_debug("BLE_GAP_EVT_DATA_LENGTH_UPDATE");
        }
        else if(BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST == event.header.evt_id)
        {
            log_debug("BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST");
            /* code below is left here just for future reference in case we need
             * to deal with data length update.
             **/
            ble_gap_data_length_params_t dl_params;
            memset(&dl_params, 0, sizeof(ble_gap_data_length_params_t));
            uint32_t err_code = sd_ble_gap_data_length_update(
            event.evt.gap_evt.conn_handle, &dl_params, NULL);
            if(NRF_SUCCESS != err_code)
                log_error("Data length update: %d", err_code);
        }
        else if(BLE_GAP_EVT_SEC_PARAMS_REQUEST == event.header.evt_id)
            log_debug("[%ld]BLE gap: sec params request", e->time);
        else if(BLE_GAP_EVT_SEC_INFO_REQUEST == event.header.evt_id)
            log_debug("[%ld]BLE gap: sec info request", e->time);
        else if(BLE_GAP_EVT_PASSKEY_DISPLAY == event.header.evt_id)
            log_debug("[%ld]BLE gap: pass key display", e->time);
        else if(BLE_GAP_EVT_KEY_PRESSED == event.header.evt_id)
            log_debug("[%ld]BLE gap: key pressed", e->time);
        else if(BLE_GAP_EVT_AUTH_KEY_REQUEST == event.header.evt_id)
        {
			log_debug("[%ld]BLE gap: authentication key request", e->time);

		}
        else if(BLE_GAP_EVT_LESC_DHKEY_REQUEST == event.header.evt_id)
            log_debug("[%ld]BLE gap: lesc dhkey request", e->time);
        else if(BLE_GAP_EVT_AUTH_STATUS == event.header.evt_id)
            log_debug("[%ld]BLE gap: authentication status", e->time);
        else if(BLE_GAP_EVT_CONN_SEC_UPDATE == event.header.evt_id)
            log_debug("[%ld]BLE gap: conn sec update", e->time);
        else if(BLE_GAP_EVT_TIMEOUT == event.header.evt_id)
            log_debug("[%ld]BLE gap: timeout", e->time);
        else if(BLE_GAP_EVT_RSSI_CHANGED == event.header.evt_id)
            log_debug("[%ld]BLE gap: rssi changed", e->time);
        else if(BLE_GAP_EVT_ADV_REPORT == event.header.evt_id)
            log_debug("[%ld]BLE gap: advertise report", e->time);
        else if(BLE_GAP_EVT_SEC_REQUEST == event.header.evt_id)
            log_debug("[%ld]BLE gap: sec request", e->time);
        else if(BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST == event.header.evt_id)
            log_debug("[%ld]BLE gap: conn param update request", e->time);
        else if(BLE_GAP_EVT_SCAN_REQ_REPORT == event.header.evt_id)
            log_debug("[%ld]BLE gap: scan request report", e->time);
        else if(BLE_GATTS_EVT_HVN_TX_COMPLETE == event.header.evt_id)
        {
            call_listener(EVT_NOTIFICATION_TX, NULL, 0);
        }
        else if(BLE_GATTS_EVT_HVC == event.header.evt_id)
        {
            call_listener(EVT_INDICATION_TX, NULL, 0);
        }
        else if(BLE_GATTS_EVT_WRITE == event.header.evt_id)
        {
            uint8_t data_buffer[LEN_EVT_DATA];

            uint8_t *data, count, *buffer;
            uint16_t handle, uuid, len_data;
            handle = event.evt.gatts_evt.params.write.handle;
            data = event.evt.gatts_evt.params.write.data;
            len_data = event.evt.gatts_evt.params.write.len;
            uuid = event.evt.gatts_evt.params.write.uuid.uuid;

            //long writes generate write events with 0 in these fields
            if((handle == 0) && (len_data == 0) && (uuid == 0))
                return;

            buffer = data_buffer;
            //copy handle
            count = sizeof(handle);
            memcpy(buffer, &handle, count);
            //copy uuid
            buffer += count;
            count = sizeof(uuid);
            memcpy(buffer, &uuid, count);
            buffer += count;
            //data length
            count = sizeof(len_data);
            memcpy(buffer, &len_data, count);
            buffer += count;

            //find header length
            count = sizeof(handle)+sizeof(uuid)+sizeof(len_data);
            //not enough space to copy the data
            if(len_data > (LEN_EVT_DATA-count))
                memset(buffer, 0xFF, (LEN_EVT_DATA-count));
            else
                memcpy(buffer, data, len_data);
            if(UUID_CCCD == uuid)
            {
                uint16_t cccd_value = 0;
                memcpy(&cccd_value, data, sizeof(cccd_value));
                log_debug("CCCD write. hnd: 0x%04X, 0x%04X",handle,cccd_value);
                if(cccd_value < 0x0004)
                    hal_ble_update_cccd(handle, cccd_value);
            }
            else
                log_debug("BLE gatts: write. uuid: 0x%04X", uuid);
            call_listener(EVT_HANDLE_WRITE, data_buffer, count+len_data);
        }
        else if(BLE_EVT_USER_MEM_REQUEST == event.header.evt_id)
        {
            log_debug("BLE gatts: user memory request event");
        }
        else if(BLE_EVT_USER_MEM_RELEASE == event.header.evt_id)
        {
            uint16_t handle, len;
            handle = event.evt.gatts_evt.params.write.handle;
            len = event.evt.gatts_evt.params.write.len;
            log_debug("BLE long char written. hnd: 0x%04X, len: %d",handle,len);
            call_listener(EVT_LONG_WRITE, &handle, sizeof(handle));
        }
        else
            log_debug("[%ld]BLE gap: unknown event %d", e->time,
                      event.header.evt_id);
    }
    else if(BLEEVT_CONNECTION == e->id)
    {
        ble_conn_params_evt_t event;
        memcpy(&event, e->data, e->len);
        if(event.evt_type == BLE_CONN_PARAMS_EVT_SUCCEEDED)
            log_debug("[%ld]BLE conn params succeeded", e->time);
        else if(event.evt_type == BLE_CONN_PARAMS_EVT_FAILED)
        {
			diag_inc_flag(FLG_BLE_UPDATE_FAILED);
			log_debug("BLE conn params failed");
		}
    }
    else if(BLEEVT_CONNECTION_ERROR == e->id)
    {
        uint32_t error;
        memcpy(&error, e->data, e->len);
        log_debug("[%ld]BLE connection error %d", e->time, error);
    }
    else if(BLEEVT_ADVERTISE == e->id)
    {
        ble_adv_evt_t event;
        memcpy(&event, e->data, e->len);
        if(BLE_ADV_EVT_IDLE == event)
            log_debug("[%ld]BLE advertising: idle", e->time);
        else if(BLE_ADV_EVT_DIRECTED == event)
            log_debug("[%ld]BLE advertising: directed", e->time);
        else if(BLE_ADV_EVT_FAST == event)
            log_debug("[%ld]BLE advertising: fast", e->time);
        else if(BLE_ADV_EVT_SLOW == event)
            log_debug("[%ld]BLE advertising: slow", e->time);
        else if(BLE_ADV_EVT_FAST_WHITELIST == event)
            log_debug("[%ld]BLE advertising: fast whitelist", e->time);
        else if(BLE_ADV_EVT_SLOW_WHITELIST == event)
            log_debug("[%ld]BLE advertising: slow whitelist", e->time);
        else if(BLE_ADV_EVT_WHITELIST_REQUEST == event)
            log_debug("[%ld]BLE advertising: whitelist request", e->time);
        else if(BLE_ADV_EVT_PEER_ADDR_REQUEST == event)
            log_debug("[%ld]BLE advertising: peer addr request", e->time);
        else
            log_debug("[%ld]BLE advertising: unknown %d", e->time, event);
    }
    else if(BLEEVT_SYSTEM == e->id)
    {
        uint32_t sysevt;
        memcpy(&sysevt, e->data, e->len);
        log_debug("[%ld]BLE connection error %d", e->time, sysevt);
    }
    else
        log_debug("[%ld]BLE unknown event %d", e->time, e->id);
}

static void ble_irq(BLEEvent id, void *data, uint16_t len)
{
    EventItem e;
    BaseType_t enqueue, cswitch = pdFALSE;

    if(len > MAX_EVT_LEN)
    {
    	diag_inc_flag(FLG_BLE_EVT_LEN);
        return;
    }
    e.time = xTaskGetTickCountFromISR();
    e.id = id;
    e.len = len;
    memcpy(e.data, data, len);
    wdt_unblock_task(wdt);
    if(is_irq_context())
        enqueue = xQueueSendToBackFromISR(evt_queue, &e, &cswitch);
    else
        enqueue = xQueueSendToBack(evt_queue, &e, DONT_WAIT);
    if(!enqueue)
    {
        wdt_feed(wdt);
        diag_inc_flag(FLG_BLE_EVT_QUEUE_FULL);
    }

    portYIELD_FROM_ISR(cswitch);
}

static void tsk_ble_events(void *params)
{
    BaseType_t dequeue;
    EventItem evt;

    while(1)
    {
        dequeue = xQueueReceive(evt_queue, &evt, WAIT_FOREVER);
        wdt_feed(wdt);
        if(pdPASS == dequeue)
        {
            dispatch_event(&evt);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
