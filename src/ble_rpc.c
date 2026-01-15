/*
 * ble_rpc.c
 *
 * Revision History:
 * Nov. 1, 2016 - Tiago Gabardo		Created the file
 * Nov. 8, 2017 - Dingchen Zhang	Added characteristics for IMU features
 *
 *  Created on: Nov 1, 2016
 *      Author: Myant
 */

#include <string.h>
#include "appconfig.h"
#include "ble.h"
#include "ble_rpc.h"

#include "ble_evt.h"
#include "logger.h"
#include "logio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tskctrl.h"
#include "semphr.h"
#include "ble_advertising.h"
#include "ble_gap.h"
#include "mya_str.h"
#include "heat_ctrl.h"
#include "pmic.h"
#include "nrf_sdm.h"
#include "wdt.h"
#include "ble_dfu.h"
#include "diagnostic.h"
#include "tps65987.h"
#include "system.h"
#include "bandcfg.h"
#include "system.h"
#include "temperature_ads.h"
#include "heater.h"

#define LEN_SERIAL 6  //the length of module serial number

//constants to define characteristics length (in bytes)
#define LENCHR_MODINFO			20
#define LENCHR_COMMAND			20
#define LENCHR_RESPONSE			20

/*<< Confirm with Tiago>> Is this going to be an issue
 * since the constant value -DMYANT_BLE_MTU = 58 in Makefile,
 * The f/w has been tested with a DCH board and seems
 * to be working fine*/
#if PCB_ID == PCB_MULTI_CHANNEL
#define LENCHR_HEATINFO_LEN		55

#elif PCB_ID == PCB_DUAL_CHANNEL
#define LENCHR_HEATINFO_LEN		22
#endif

#define HEATINFO_MAX_LEN    	55

//maximum of messages that can be stored in the push buffer
#define N_PUSH_ITEM         40
#define LEN_BASE_UUID       16
#define MAX_SERVICES        1
#define LEN_ADV_DATA        6
#define MAX_INFO		    3
typedef void(*parser_t)(void);

#define BASE_UUID           {0x1F,0xE8,0x52,0x84,0x14,0x04,0x21,0x9D,\
                             0xA8,0x4E,0x30,0x96,0x00,0x00,0x04,0x20}
#define UUID_SKIIN_SERVICE  SKIIN_MODULE_ID
#define UUID_MODINFO        0x6801
#define UUID_COMMAND        0x6802
#define UUID_RESPONSE       0x6803
#define UUID_LOGGER         0x6804
#define UUID_HEATINFO       0x6833

typedef struct
{
    CharMode mode;
    uint8_t len;
    uint16_t handle;
    uint8_t data[MAX_LEN_CHAR];
}push_item_t;

static bool is_connected = false;
static bool config_ok = false;
static uint8_t base_uuid[] = BASE_UUID;
static uint16_t uuid_services[MAX_SERVICES] = {0};
static uint16_t ct_services = 0;
static uint16_t conn_interval = 0;
static uint8_t free_tx_slots = 0;
static QueueHandle_t push_queue = NULL;
static SemaphoreHandle_t mtx_ntf = NULL;
//data to be advertised. check set_adv_data()
static uint8_t adv_data[LEN_SERIAL] = {0};
/* todo create an array of service handlers. this temporary solution only works
 * for 1 service. Check also the add_characteristic() because it uses service
 * handlers */
static uint16_t srv_handle = 0;

static uint8_t heatinfo_data[HEATINFO_MAX_LEN] = {0};
static channel_ctl_t channel_data[MAX_HEATERS] = {0};

static void reset_settings(void);
static void tsk_push_messages(void *params);
static bool add_chars(void);
static bool add_services(void);
static void ble_event_irq(evt_id_t id, void *data, uint16_t len);
static void set_adv_data(void);
static bool add_dfu_service(void);
static void dfu_evt_handler(ble_dfu_buttonless_evt_type_t event);
static inline void print_char(uint16_t handle, uint8_t *data, uint16_t len)
__attribute__ ((unused));

static uint8_t buffer_cmd[LENCHR_COMMAND] = {0};

//shortcuts to characteristics modes
#define _R	BLEMODE_READ
#define _W	BLEMODE_WRITE
#define _N	BLEMODE_NOTIFY
#define _I	BLEMODE_INDICATE

//local characteristics
static CharData local_chars[] =
{
    {BASE_UUID, UUID_MODINFO, 0, 0, LENCHR_MODINFO, NULL, 0, _R+_N},
    {BASE_UUID, UUID_COMMAND, 0, 0,	LENCHR_COMMAND, buffer_cmd, 0, _W},
    {BASE_UUID, UUID_RESPONSE, 0, 0, LENCHR_RESPONSE, NULL, 0, _I},
    {BASE_UUID, UUID_LOGGER, 0, 0, LENCHR_LOGGER, NULL, 0, _N},
	{BASE_UUID, UUID_HEATINFO, 0, 0, LENCHR_HEATINFO_LEN, NULL, 0, _R+_N},
};

static const uint16_t NLOCAL_CHARS = sizeof(local_chars)/sizeof(CharData);
static const uint8_t zeros[MAX_LEN_CHAR] = {0};
static CharData *chrcmd = NULL;
static wdt_entry wdt = NULL;

static uint8_t modinfo_data[LENCHR_MODINFO] = {0};
bool ble_init(void)
{
    BLEStatus blest;
    TaskHandle_t hnd;
    wdt_config_t wdtconfig;

    if(MAX_BLE_CHARS > MAX_SUPPORTED_CHARS)
    {
        log_error("MAX_SUPPORTED_CHARS in ble driver must be updated!");
        return false;
    }

    hnd = tsk_create(TSK_BLE_PUSH, tsk_push_messages);
    ASSERT_TASK(hnd);
    push_queue = os_create_queue(N_PUSH_ITEM, sizeof(push_item_t));
    ASSERT_QUEUE(push_queue);
    mtx_ntf = xSemaphoreCreateMutex();
    ASSERT_MUTEX(mtx_ntf);
    ble_evt_init();
    ble_evt_add_calback(ble_event_irq);
    reset_settings();

    //configure watchdog timer
    wdtconfig.enabled = true;
    wdtconfig.event_mask = 0;
    wdtconfig.id = TSK_BLE_PUSH;
    wdtconfig.timeout = 2;
    wdtconfig.type = WDT_E_BLOCKING;
    wdt = wdt_config_entry(&wdtconfig, pdMS_TO_TICKS(200));

    //set module name. use modinfo_data just for convenience
    memset(modinfo_data, 0, sizeof(modinfo_data));
    memcpy(modinfo_data, BLE_NAME, 4);

    blest = hal_ble_init((char*)modinfo_data);
    if(BLEST_OP_OK == blest)
    {
        if(add_services())
        {
            config_ok = add_chars();
            if(config_ok)
            {
                chrcmd = ble_get_char(BLEMSG_COMMAND);
                ble_connect();
                ble_update_modinfo(false);
            }
        }
        //add dfu service
		if(!add_dfu_service())
			log_error("failed to add DFU service");
    }
    else
	{
		diag_inc_flag(FLG_BLE_INIT_HAL);
		log_error("Failed to init hal_ble");
	}

    return config_ok;
}

static uint8_t get_overheat_state(void)
{
	uint8_t overheat = 0;

	overheat  = 0; //<<hack>> get real overheat status
	overheat |= (heat_is_channel_overheating(HEATER_A) << 1);
	overheat |= (heat_is_channel_overheating(HEATER_B) << 2);
#if PCB_ID == PCB_MULTI_CHANNEL
	overheat |= (heat_is_channel_overheating(HEATER_C) << 3);
	overheat |= (heat_is_channel_overheating(HEATER_D) << 4);
	overheat |= (heat_is_channel_overheating(HEATER_E) << 5);
#endif
	return overheat;
}

BLEStatus ble_update_modinfo(bool notify)
{
    BLEStatus blest;
    uint16_t temp=0;

    CharData *info = ble_get_char(BLEMSG_MODINFO);
    if(NULL == info)
        return BLEST_OP_FAIL;
    memset(modinfo_data, 0, sizeof(modinfo_data));
    //hardware version
    modinfo_data[0] = HW_VERSION;
    //nrf52 firmware version [1..4]
    modinfo_data[1] = FWV_MAJOR;
    modinfo_data[2] = FWV_MINOR;
    modinfo_data[3] = FWV_PATCH;
    modinfo_data[4] = FWV_BUILD;
    modinfo_data[5]= band_get_garmentid();
    modinfo_data[6]= pmic_get_battery_range();
    modinfo_data[7]= (uint8_t)hw_get_duty_period_count();
    /* list 7 voltage options that power bank can support */
	for(uint8_t i=0;i<7;i++)
	{
		modinfo_data[8+i]=tps_retrieve_voltagelevel(i);
	}
	modinfo_data[15] = 0; //no_motion_st
	temp=(uint16_t)ads_get_PCB_temperature();
	memcpy(&(modinfo_data[16]),&temp,sizeof(temp));
	//check overheating states
	modinfo_data[18] = get_overheat_state();

	/* update a characteristic without sending a notification, so app
     * can read this characteristic */
    blest = hal_ble_update_chr(info->handle, modinfo_data, info->len);
    if((BLEST_OP_OK == blest) && notify && ble_is_notification_on(BLEMSG_MODINFO))
        blest = ble_notify(BLEMSG_MODINFO, modinfo_data, info->len);
    return blest;
}

#define HEAT_CH_INFO_LEN   11

BLEStatus ble_update_heatinfo(bool notify)
{
    uint8_t i=0;
    uint16_t temp;
    uint16_t session_remaining_time;
    BLEStatus blest;

    CharData *info = ble_get_char(BLEMSG_HEATINFO);
    if(NULL == info)
    	return BLEST_OP_FAIL;

    memset(heatinfo_data, 0, sizeof(heatinfo_data));

    for(uint8_t ch = HEATER_A; ch < MAX_HEATERS; ch++)
    {
       	if(!heat_get_channel_params(ch, &channel_data[ch]))
       		return BLEST_INVALID_PARAMS;
    }

    for(uint8_t ch = HEATER_A; ch < MAX_HEATERS; ch++)
    {
       	i = (ch*HEAT_CH_INFO_LEN);
       	heatinfo_data[i++]=(uint8_t)hw_get_ch_ontime_count(ch);
       	heatinfo_data[i++]=(uint8_t)(channel_data[ch].measures.status);
       	heatinfo_data[i++]=(uint8_t)(channel_data[ch].measures.volts/100);
       	heatinfo_data[i++]=(uint8_t)(channel_data[ch].measures.resistance/100);
       	temp = (uint16_t)(channel_data[ch].measures.temperature*100);
       	memcpy(&heatinfo_data[i],&temp,sizeof(temp));
       	heatinfo_data[i+2]=(uint8_t)(channel_data[ch].temperature_setpoint);
       	session_remaining_time = (uint16_t)heat_get_remaining_session_time();
       	memcpy(&heatinfo_data[i+3],&session_remaining_time,sizeof(session_remaining_time));
       	memcpy(&heatinfo_data[i+5],&channel_data[ch].measures.current,sizeof(uint16_t));
    }

    blest = hal_ble_update_chr(info->handle, heatinfo_data, info->len);
    if((BLEST_OP_OK == blest) && notify && ble_is_notification_on(BLEMSG_HEATINFO))
    	blest = ble_notify(BLEMSG_HEATINFO, heatinfo_data, info->len);

    return blest;
}


BLEStatus ble_update_battery_level(uint8_t level)
{
    if(!ble_is_connected())
        return BLEST_NOT_CONNECTED;
    return hal_ble_set_battery_level(level);
    return BLEST_OP_OK;
}

bool ble_is_config_ok(void)
{
    return config_ok;
}

static void reset_settings(void)
{
    is_connected = false;
    conn_interval = 0;
    srv_handle = 0;
    ct_services = 0;
    free_tx_slots = 0;
    config_ok = false;
    memset(uuid_services, 0, sizeof(uuid_services));
}

CharData *ble_get_char(ble_msgid_e id)
{
    if(id >= NLOCAL_CHARS)
    {
        log_error("Trying to get invalid local char: %d", id);
        return NULL;
    }
    /*
     * The order of element in enumeration 'ble_msgid_e' is required to be
     * consistent to that of entry in array 'local_chars[]'
     */
    CharData *chr = &local_chars[id];
    return chr;
}

uint16_t ble_get_conn_interval(void)
{
    return conn_interval;
}

//set advertising data
static void set_adv_data(void)
{
    BLEStatus blest;
    static uint8_t serial_number[LEN_SERIAL] = {10};
	memset(adv_data, 0, sizeof(adv_data));

	//<<hack>> read serial number from uicr
	memcpy(&adv_data, serial_number, sizeof(serial_number));
    blest = hal_ble_set_advdata(adv_data, LEN_ADV_DATA);
    if(BLEST_OP_OK != blest)
   {
	   log_error("[ble_rpc] failed to set advertise data: %d", blest);
	   diag_inc_flag(FLG_BLE_SET_ADV_DATA);
   }
}

bool ble_is_connected(void)
{
    return is_connected;
}

bool ble_is_indication_on(ble_msgid_e msgid)
{
    uint16_t cccdvalue;
    CharData *chr = ble_get_char(msgid);
    if(NULL == chr)
        return false;
    cccdvalue = hal_ble_get_cccd(chr->handle);
    return (cccdvalue & IND_ENABLED);
}

bool ble_is_notification_on(ble_msgid_e msgid)
{
    uint16_t cccdvalue;
    CharData *chr = ble_get_char(msgid);
    if(NULL == chr)
        return false;
    cccdvalue = hal_ble_get_cccd(chr->handle);
    return (cccdvalue & NTF_ENABLED);
}

BLEStatus ble_push(CharMode mode, uint16_t handle, void *data, uint8_t len)
{
    push_item_t item;
    BaseType_t enqueued;
    BLEStatus status = BLEST_OP_FAIL;

    if(!is_connected)
        status = BLEST_NOT_CONNECTED;
    else if(uxQueueSpacesAvailable(push_queue) < 1)
    {
        status = BLEST_QUEUE_FULL;
        diag_inc_flag(FLG_BLE_PUSHQ_FULL);
        log_error("no space in push queue, %s, %d", __FILE__, __LINE__);
    }
    else if(len <= MAX_LEN_CHAR)
    {
        if(!hal_ble_is_push_on_by_char_hnd(handle)) //push not enabled
            return BLEST_NOT_ENABLED;
        item.handle = handle;
        item.len = len;
        item.mode = mode;
        memcpy(item.data, data, len);
        wdt_unblock_task(wdt);
        enqueued = xQueueSendToBack(push_queue, &item, DONT_WAIT);
        if(pdPASS == enqueued)
            status = BLEST_OP_OK;
        else
        {
            wdt_feed(wdt);
            diag_inc_flag(FLG_BLE_PUSH_ENQUEUE);
            log_error("%s, %d", __FILE__, __LINE__);
            status = BLEST_OP_FAIL;
        }
    }
    else
        status = BLEST_LEN_INVALID;
    return status;
}

BLEStatus ble_notify(ble_msgid_e id, void *data, uint8_t len)
{
    CharData *chr = ble_get_char(id);
    if(NULL == chr)
        return BLEST_INVALID_ID;
    return ble_push(BLEMODE_NOTIFY, chr->handle, data, len);
}

BLEStatus ble_try_notify(ble_msgid_e id, void *data, uint8_t len,
                         uint8_t attempts)
{
    uint8_t counter = 0;
    BLEStatus txst = BLEST_OP_FAIL;

    if(NULL == data)
        return BLEST_INVALID_PARAMS;
    //send message to BLE notification queue
    while((BLEST_OP_OK != txst) && (BLEST_QUEUE_FULL != txst) &&
    	  (BLEST_LEN_INVALID != txst) && (counter < attempts))
    {
        counter++;
        txst = ble_notify(id, data, len);
        if((txst != BLEST_OP_OK) && (txst != BLEST_QUEUE_FULL))
            vTaskDelay(pdMS_TO_TICKS(1));
    }
    return txst;
}

BLEStatus ble_notify_modinfo(void)
{
	CharData *chr = &local_chars[BLEMSG_MODINFO];
	BLEStatus txst = BLEST_OP_FAIL;
	uint8_t attemps = 0;
	while( (txst!=BLEST_OP_OK) &&
		   (txst!=BLEST_QUEUE_FULL) &&
		   attemps++<10)
	{
		txst = ble_push(BLEMODE_NOTIFY, chr->handle, modinfo_data, 20);
	}
	if( txst!=BLEST_OP_OK )
		log_error("modinfo notify fail");
	return txst;
}

BLEStatus ble_indicate(ble_msgid_e id, void *data, uint8_t len)
{
    CharData *chr = ble_get_char(id);
    if(NULL == chr)
        return BLEST_INVALID_ID;
    return ble_push(BLEMODE_INDICATE, chr->handle, data, len);
}

static bool add_dfu_service(void)
{
    ble_dfu_buttonless_init_t dfus_init =
    {
        .evt_handler = dfu_evt_handler
    };
    uint32_t err_code = ble_dfu_buttonless_init(&dfus_init);
    if(NRF_SUCCESS != err_code)
    {
        log_error("[ble_rpc] ble dfu init failed %d: ", err_code);
        return false;
    }
    return true;
}

static bool add_services(void)
{
    uint16_t handle;
    ServiceData srvdata;
    BLEStatus status;

    memcpy(srvdata.uuid128, base_uuid, LEN_BASE_UUID);
    srvdata.uuid16 = UUID_SKIIN_SERVICE;
    status = hal_ble_add_service(&srvdata, &handle);

    if(BLEST_OP_OK == status)
    {
        uuid_services[ct_services] = srvdata.uuid16;
        srv_handle = handle;
        ct_services++;
        log_debug("service 0x%04X added", srvdata.uuid16);
    }
    else
        log_error("Failed to add service: 0x%04X", srvdata.uuid16);
    return (BLEST_OP_OK == status);
}

static void dfu_evt_handler(ble_dfu_buttonless_evt_type_t event)
{
    switch(event)
    {
        case BLE_DFU_EVT_BOOTLOADER_ENTER_PREPARE:
            log_debug("[ble_rpc]: prepare to enter bootloader");
        break;
        case BLE_DFU_EVT_BOOTLOADER_ENTER:
            log_debug("[ble_rpc]: enter bootloader");
        break;
        case BLE_DFU_EVT_BOOTLOADER_ENTER_FAILED:
            log_error("[ble_rpc]: failed to enter bootloader");
        break;
        default:
            log_error("[ble_rpc]: unknown event from ble_dfu: %d", event);
        break;
    }
}

static bool add_chars(void)
{
    if(!ct_services)
        return false;
    uint8_t i;
    uint16_t handle;
    BLEStatus st;
    CharData *chr = local_chars;
    if(NLOCAL_CHARS != MAX_BLE_CHARS)
    {
        log_error("Wrong local chars definitions");
        return false;
    }
    for(i = 0; i < NLOCAL_CHARS; i++, chr++)
    {
        //set characteristic uuid
        memcpy(chr->uuid128, base_uuid, LEN_BASE_UUID);
        chr->hnd_service = srv_handle;
        st = hal_ble_add_characteristic(chr, (void*)zeros, &handle);
        if(BLEST_OP_OK == st)
            chr->handle = handle;
        else
        {
            log_error("Failed to add local char 0x%04X, %d", chr->uuid, st);
            return false;
        }
    }
    return true;
}

bool ble_connect(void)
{
    BLEStatus status = BLEST_OP_FAIL;
    if(ct_services)
    {
        status = hal_ble_connect();
        if (BLEST_OP_OK == status)
        {
            ble_advertise_start();
            return true;
        }
    }
    return false;
}

void ble_advertise_start(void)
{
    BLEStatus status = BLEST_OP_FAIL;
    set_adv_data();
    status = hal_ble_advertise_start(uuid_services, ct_services);
    if(BLEST_OP_OK != status)
    {
    	diag_inc_flag(FLG_BLE_ADV_ERROR);
        log_error("Advertising error: %ld, %s, %d", status, __FILE__, __LINE__);
    }
}

void ble_clean_push_queue(void)
{
    if(push_queue)
    {
        xQueueReset(push_queue); //flush queue
        wdt_feed(wdt);
     }
}

#define MIN_PUSH_DELAY      5
#define MAX_PUSH_ATTEMPTS   20
static void tsk_push_messages(void *params)
{
    BaseType_t dequeue, ntfok;
    push_item_t item;
    BLEStatus status;
    TickType_t pushdelay = MIN_PUSH_DELAY;
    uint16_t attempts = 0, ct_dropped = 0;
    uint32_t ntfval;
    uint8_t used_tx_slots = 0;
    bool suspend_request = true;

    xTaskNotifyWait(0xFFFFFFFF, 0xFFFFFFFF, &ntfval, DONT_WAIT);
    while(1)
    {
        if(!is_connected || suspend_request)
        {
            suspend_request = false;
            vTaskSuspend(self_task());
            pushdelay = conn_interval>>1;
            if(pushdelay < MIN_PUSH_DELAY)
                pushdelay = MIN_PUSH_DELAY;
            ble_clean_push_queue();
            attempts = 0;
            used_tx_slots = 0;
            xTaskNotifyWait(0xFFFFFFFF, 0xFFFFFFFF, &ntfval, DONT_WAIT);
        }
        dequeue = xQueuePeek(push_queue, &item, WAIT_FOREVER);
        wdt_feed(wdt);
        if(pdPASS == dequeue)
        {
            ntfok = xTaskNotifyWait(0, 0, &ntfval, DONT_WAIT);
            if(pdTRUE == ntfok)
            {
                if(used_tx_slots < ntfval)
                    used_tx_slots = 0;
                else
                    used_tx_slots -= ntfval;
            }
            if(used_tx_slots < free_tx_slots)
            {
                if(!hal_ble_is_push_on_by_char_hnd(item.handle))
                {   //push not enabled, remove item from queue
                    xQueueReceive(push_queue, &item, DONT_WAIT);
                    continue; //next while loop
                }
                attempts++;
                status = hal_ble_push(item.mode, item.handle, item.data, item.len);
                if((BLEST_NOT_ENABLED == status) ||
                   (BLEST_NOT_CONNECTED == status))
                {
                    suspend_request = true;
                    continue; //next while loop
                }
                if((BLEST_OP_OK != status) && is_connected)
                {
                    if(attempts > MAX_PUSH_ATTEMPTS)
                    {
                    	diag_inc_flag(FLG_BLE_PUSHTSK_ERROR);
                        log_error("Fail to push message: %d", item.handle);
                        attempts = 0;
                        ct_dropped++;
                    }
                    vTaskDelay(pdMS_TO_TICKS(pushdelay));
                }
                else//remove item from queue
                {
                    used_tx_slots++;
                    attempts = 0;
                    xQueueReceive(push_queue, &item, DONT_WAIT);
                }
            }
        }
    }
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

#define MAX_EVT_LEN     27
#define MAX_ATTEMPTS    10
static void ble_event_irq(evt_id_t id, void *data, uint16_t len)
{
    if((len+1) > MAX_EVT_LEN)
    {
    	diag_inc_flag(FLG_BLE_MAX_EVENTS);
    	log_error("BLE event buffer overflow. %s, %d", __FILE__, __LINE__);
        return;
    }

    if(id == EVT_CONN_UPDATE)
    {
        if(sizeof(conn_interval) == len) //update connection interval
        {
            memcpy(&conn_interval, data, len);
            conn_interval *= 1.25;
            os_evt_trigger(EVT_BLE_CONN_UPDATE);
        }
    }
    if(id == EVT_CONNECTED)
    {
        is_connected = true;
        if(sizeof(conn_interval) == len) //update connection interval
        {
            memcpy(&conn_interval, data, len);
            conn_interval *= 1.25;
        }
        xSemaphoreTake(mtx_ntf, WAIT_FOREVER);
        free_tx_slots = BLE_UUID_VS_COUNT_MAX;
        xSemaphoreGive(mtx_ntf);
        if(tsk_state_by_id(TSK_BLE_PUSH) == eSuspended)
            tsk_resume_by_id(TSK_BLE_PUSH);
        os_evt_trigger(EVT_BLE_CONNECTED);
    }
    else if(id == EVT_DISCONNECTED)
    {
        conn_interval = 0;
        is_connected = false;
        hal_ble_reset_all_cccd();
        ble_clean_push_queue();
        os_evt_trigger(EVT_BLE_DISCONNECTED);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    else if((EVT_NOTIFICATION_TX == id) || (EVT_INDICATION_TX == id))
    {
        TaskHandle_t tsk_push = tsk_get_handle(TSK_BLE_PUSH);
        if(tsk_push)
            xTaskNotify(tsk_push, 0, eIncrement);
        return;
    }
    else if(EVT_HANDLE_WRITE == id)
    {
        fmt_st_e st;
        bleevt_callb_data_t evtdata;
        st = ble_evt_format_data(id, &evtdata, data, len);
        if(FMTST_OK != st)
            return;
        if(UUID_CCCD == evtdata.uuid)
            os_evt_trigger(EVT_BLE_CCCD_WRITE);
        else if(UUID_COMMAND == evtdata.uuid)
        {
            if(NULL != chrcmd)
            {
                if(chrcmd->len < evtdata.len_data)
                {
                	diag_inc_flag(FLG_BLE_CMD_OVF);
                    log_error("BLE command overflow. len: %d, received: %d",
                            chrcmd->len, len);
                    return;
                }
                memcpy(chrcmd->data, evtdata.data, evtdata.len_data);
                chrcmd->validlen = evtdata.len_data;
                os_evt_trigger(EVT_BLE_COMMAND);
            }
            else
                log_error("BLE command char not initialized");
        }
        else
            os_evt_trigger(EVT_BLE_CHAR_WRITE);
    }
    else if(EVT_LONG_WRITE == id)
    {
        uint16_t handle = 0;
        if(len >= sizeof(handle))
            memcpy(&handle, data, sizeof(handle));
        log_debug("Received long write evt on handle: 0x%04X", handle);
    }
    else
    {
        log_debug("BLE RPC event unknown event id: %d", id);
    }
}
