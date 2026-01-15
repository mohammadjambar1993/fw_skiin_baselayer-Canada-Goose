/*
 * hal_ble_nrf52.c
 *
 *  Created on: Oct 3, 2016
 *      Author: Myant
 */

#include "hal_ble.h"
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833) && (ENABLE_HAL_BLE == 1)

#include <string.h>
#include "appconfig.h"
#include "logio.h"
#include "logger.h"
#include "app_timer.h"
#include "nrf_sdm.h"
#include "ble_advertising.h"
#include "ble_hci.h"
#include "ble_gap.h"
#include "ble_conn_params.h"
#include "app_util.h"
#include "peer_manager.h"
#include "mya_util.h"
#include "mya_str.h"
#include "../diagnostic.h"
#include "nordic_common.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "nrf_ble_gatt.h"
#include "nrf_sdh_soc.h"
#include "peer_manager_handler.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"

/**< A tag identifying the SoftDevice BLE configuration. */
#define APP_BLE_CONN_CFG_TAG                1
/*< Application's BLE observer priority. You shouldn't need to modify this
 * value. */
#define APP_BLE_OBSERVER_PRIO               3
/**< Advertising module instance. */
BLE_ADVERTISING_DEF(m_advertising);
/**< GATT module instance. */
NRF_BLE_GATT_DEF(m_gatt);

#define ROLE_CENTRAL        0
#define ROLE_PERIPHERAL     1
//number of supported chars with push (notification/indication) enabled
#define N_LONG_WRITE        192 //length of buffer used to receive long writes

#define TIME_BASE                        1.25    //milliseconds
#define MAX_BYTE_PACK                    20      //maximum bytes per packet
#define MAX_TX_SLOTS                     6
#define MIN_CONN_INTERVAL                6      //*1.25ms
#define MAX_CONN_INTERVAL                BLE_GAP_CP_MAX_CONN_INTVL_MAX
//Slave latency
#define SLAVE_LATENCY                    0
//Connection supervisory timeout (4 seconds)

#define STD_MTU 23

#define CONN_SUP_TIMEOUT                 BLE_GAP_CP_CONN_SUP_TIMEOUT_MAX
#define MAX_ADV_DATALEN     29  //maximum user advertising data length

typedef struct
{
    uint16_t hnd_value;
    uint16_t hnd_cccd;
    uint16_t value_cccd;
    uint16_t uuid16;
}char_ctrl_t;

static void reset_settings(void);
static bool stack_init(void);
static bool gap_init(char *name);
static void ble_events_dispatch(ble_evt_t const *event, void *p_context);
static void on_adv_evt(ble_adv_evt_t ble_adv_evt);
static void on_connection_events(ble_conn_params_evt_t *evt);
static void on_connection_error(uint32_t error);
static void call_listener(BLEEvent id, void *data, uint16_t len);
static void pm_evt_handler(pm_evt_t const * p_event);
static void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt);

static bool configured = false;
static bool connected = false;
static bool link_secured = false;
static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID; //connection handle
//Application identifier allocated by device manager
static void(*listener)(BLEEvent id, void *data, uint16_t len) = NULL;
static char_ctrl_t *get_cccd_by_hnd(uint16_t hnd);
static char_ctrl_t *get_cccd_by_charhnd(uint16_t hnd);
static char_ctrl_t *get_char_by_uuid(uint16_t uuid);
static void update_adv_data(void);
static void gatt_init(void);

//variables to control characteristics with notification/indication enabled
static uint16_t num_chars = 0;
static char_ctrl_t chars_ctrl[MAX_SUPPORTED_CHARS] = {{0}};
//variables used to control advertising data
static uint8_t advdata[MAX_ADV_DATALEN] = {0};
static uint8_t advdata_len = 0;
static int8_t txpower = BLE_TX_POWER;
static ble_advdata_service_data_t srvdata;
static uint8_array_t array_advdata;
static ble_advdata_uuid_list_t adv_srv_uuid;
static ble_uuid_t uuid_advdata;
static uint16_t mtu_effective = STD_MTU;

#define CHECK_STATUS(CODE, BLINKS) do{                          \
if(NRF_SUCCESS != CODE)                                         \
{                                                               \
    log_error("[sd] code: %d code, %s, %d", __FILE__, __LINE__);\
    return BLEST_OP_FAIL;                                       \
}                                                               \
}while(0)

static char_ctrl_t *get_cccd_by_hnd(uint16_t hnd)
{
    if(num_chars <= 0)
        return NULL;
    uint8_t i;
    char_ctrl_t *ctrl = chars_ctrl;
    for(i = 0; i < MAX_SUPPORTED_CHARS; i++, ctrl++)
    {
        if(ctrl->hnd_cccd == hnd)
            return ctrl;
    }
    return NULL;
}

static char_ctrl_t *get_cccd_by_charhnd(uint16_t hnd)
{
    if(num_chars <= 0)
        return NULL;
    uint8_t i;
    char_ctrl_t *ctrl = chars_ctrl;
    for(i = 0; i < MAX_SUPPORTED_CHARS; i++, ctrl++)
    {
        if(ctrl->hnd_value == hnd)
            return ctrl;
    }
    return NULL;
}

static char_ctrl_t *get_char_by_uuid(uint16_t uuid)
{
    if(num_chars <= 0)
        return NULL;
    uint8_t i;
    char_ctrl_t *ctrl = chars_ctrl;
    for(i = 0; i < MAX_SUPPORTED_CHARS; i++, ctrl++)
    {
        if(ctrl->uuid16 == uuid)
            return ctrl;
    }
    return NULL;
}

uint16_t hal_ble_get_cccd(uint16_t char_handle)
{
    char_ctrl_t *chr = get_cccd_by_charhnd(char_handle);
    if(NULL == chr)
        return 0;
    return chr->value_cccd;
}

static void reset_settings(void)
{
    num_chars = 0;
    connected = false;
    conn_handle = BLE_CONN_HANDLE_INVALID;
    memset(chars_ctrl, 0, sizeof(chars_ctrl));
}

static bool stack_init(void)
{
    uint32_t err_code;

    //initialize the timer used by soft device
    app_timer_init();
    //initialize ble stack
    err_code = nrf_sdh_enable_request();
    CHECK_STATUS(err_code, ERRBLINK_BLE_DEFCONFIG);

    //Check the ram settings against the used number of links
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);

    //enable ble stack
    err_code = nrf_sdh_ble_enable(&ram_start);
    CHECK_STATUS(err_code, ERRBLINK_BLE_ENABLED);
    // Register with the SoftDevice handler module for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO,
                        ble_events_dispatch, NULL);

    return true;
}

static bool peer_manager_init(bool erasebond)
{
    uint32_t           errcode;
    ble_gap_sec_params_t param_register;

    errcode = pm_init();

    memset(&param_register, 0, sizeof(ble_gap_sec_params_t));

    param_register.io_caps        = BLE_GAP_IO_CAPS_NONE;
    //maximum and minimum encription key size
    param_register.min_key_size   = 7;
    param_register.max_key_size   = 16;
#if BLE_SECURED == 1
    param_register.bond           = 1;
    param_register.mitm           = 1;
    param_register.oob            = 1;
    param_register.kdist_own.enc  = 1;
    param_register.kdist_own.id   = 1;
    param_register.kdist_peer.enc = 1;
    param_register.kdist_peer.id  = 1;
#else
    param_register.bond           = 0; //Do not perform bonding
    param_register.mitm           = 0; //mitm protection not required
    param_register.oob            = 0; //out of band data not available
#endif //BLE_SECURED

    errcode = pm_sec_params_set(&param_register);
    if(NRF_SUCCESS != errcode)
    {
        log_error("[hal_ble] failed to sec params");
        diag_inc_flag(FLG_BLE_PM_SET);
        return false;
    }
    errcode = pm_register(pm_evt_handler);
    if(NRF_SUCCESS != errcode)
    {
        log_error("[hal_ble] failed to register PM event");
        diag_inc_flag(FLG_BLE_PM_REG);
        return false;
    }
    return (NRF_SUCCESS == errcode);
}

static bool gap_init(char *name)
{
    uint32_t errcode;
    ble_gap_conn_params_t   params;

    hal_ble_set_module_name(name);
    //initialize gap params
    memset(&params, 0, sizeof(params));
    //populate the GAP connection parameter struct
    params.min_conn_interval = MIN_CONN_INTERVAL;
    params.max_conn_interval = MAX_CONN_INTERVAL;
    params.slave_latency     = SLAVE_LATENCY;
    params.conn_sup_timeout  = CONN_SUP_TIMEOUT;
    //set GAP Peripheral Preferred Connection Parameters
    errcode = sd_ble_gap_ppcp_set(&params);
    CHECK_STATUS(errcode, ERRBLINK_BLE_GAPPARAMS);
    /* set appearance (16 bit code to identify device type. Ex:
     * phone, computer, watch, etc */
    errcode = sd_ble_gap_appearance_set(0);
    CHECK_STATUS(errcode, ERRBLINK_BLE_APPEARANCE);

    return true;
}

static void gatt_init(void)
{
    ret_code_t err;
    err = nrf_ble_gatt_att_mtu_periph_set(&m_gatt,NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    if(NRF_SUCCESS != err)
        log_error("[hal_ble] failed to set MTU: %d", err);

    err = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    if(NRF_SUCCESS != err)
    {
        log_error("[hal_ble] gatt init error: %d", err);
        diag_inc_flag(FLG_BLE_GATT_INIT);
    }
}

uint16_t hal_ble_get_mtu(void)
{
    return mtu_effective;
}

static void update_adv_data(void)
{
    array_advdata.p_data = advdata;
    array_advdata.size = advdata_len;

    srvdata.service_uuid = 0x180A;
    srvdata.data = array_advdata;

    uuid_advdata.type = BLE_UUID_TYPE_VENDOR_BEGIN;
    uuid_advdata.uuid = SKIIN_MODULE_ID;

    adv_srv_uuid.uuid_cnt = 1;
    adv_srv_uuid.p_uuids = &uuid_advdata;
}

int8_t hal_ble_get_txpower(void)
{
    return txpower;
}

BLEStatus hal_ble_init_sd(void)
{
    BLEStatus status = BLEST_OP_FAIL;
    if(!configured)
    {
        if(stack_init())
        {
            status = BLEST_OP_OK;
        }
        else
        {
        	diag_inc_flag(FLG_BLE_INIT_HAL);
        	log_error("softdevice not initialized");
        }
    }
    return status;
}

BLEStatus hal_ble_deinit_sd(void)
{
    uint32_t error;
    BLEStatus status = BLEST_OP_FAIL;

    log_debug("Resetting soft device...");
    configured = false;
    reset_settings();
    error = sd_softdevice_disable();
    if(NRF_SUCCESS == error)
        log_debug("soft device disabled");
    else
    {
        log_error("failed disabling soft device: %ld", error);
    	diag_inc_flag(FLG_BLE_INIT_HAL);
    }
    status = (NRF_SUCCESS == error) ? BLEST_OP_OK : BLEST_OP_FAIL;
    return status;
}

BLEStatus hal_ble_init(char *name)
{
    bool initok = configured;
    if(!configured)
    {
        advdata_len = 0;
        hal_ble_init_sd();
        initok = peer_manager_init(true);
        initok &= gap_init(name);
        gatt_init();
    }
    configured = initok;
    if(!configured)
        return BLEST_OP_FAIL;
    return BLEST_OP_OK;
}

BLEStatus hal_ble_get_addr(uint8_t *addr, uint8_t len)
{
    if(len < BLE_GAP_ADDR_LEN)
        return BLEST_LEN_INVALID;
    memset(addr, 0, len);
    ble_gap_addr_t gapaddr;
    if(NRF_SUCCESS == sd_ble_gap_addr_get(&gapaddr))
    {
        memcpy(addr, gapaddr.addr, BLE_GAP_ADDR_LEN);
        return BLEST_OP_OK;
    }
    return BLEST_OP_FAIL;
}

BLEStatus hal_ble_get_name(uint8_t *name, uint16_t *len)
{
    uint32_t st;
    st = sd_ble_gap_device_name_get(name, len);
    if(NRF_SUCCESS == st)
        return BLEST_OP_OK;
    return BLEST_OP_FAIL;
}

BLEStatus hal_ble_add_characteristic(const CharData *charac, void *init_value,
                                     uint16_t *handle)
{
    if(!configured)
        return BLEST_OP_FAIL;

    char_ctrl_t *chr = get_char_by_uuid(charac->uuid);
    if(NULL != chr) //characteristic is already present
    {
        *handle = chr->hnd_value;
        log_debug("Char 0x%04X existent, hnd: 0x%04X", chr->uuid16, *handle);
        return BLEST_OP_OK;
    }

    uint32_t error;
    ble_uuid_t uuid16;

    //add custom characteristic UUID
    ble_uuid128_t uuid128;
    memcpy(&uuid128, charac->uuid128, 16);
    uuid16.uuid = charac->uuid;
    error = sd_ble_uuid_vs_add(&uuid128, &uuid16.type);
    CHECK_STATUS(error, ERRBLINK_BLE_CHARADD_UUID);

    //configure attribute metadata
    ble_gatts_attr_md_t attr_md;
    memset(&attr_md, 0, sizeof(attr_md));
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.vlen = 1; //variable length attribute
    //set read/write security levels to the characteristic
#if BLE_SECURED == 1
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.write_perm);
#else
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
#endif

    //configure the characteristic value attribute
    ble_gatts_attr_t attr_value;
    memset(&attr_value, 0, sizeof(attr_value));
    attr_value.p_uuid = &uuid16;
    attr_value.p_attr_md = &attr_md;
    attr_value.max_len = charac->len;
    attr_value.init_len = charac->len;
    attr_value.p_value = (uint8_t*)init_value;

    //configure Client Characteristic Configuration Descriptor (CCCD)
    ble_gatts_attr_md_t cccd_md;
    memset(&cccd_md, 0, sizeof(cccd_md));
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
#if BLE_SECURED == 1
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&cccd_md.write_perm);
#else
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
#endif


    //Add read/write properties to the characteristic
    ble_gatts_char_md_t char_md;
    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.read = read_bit(charac->mode, (BLEMODE_READ-1));
    char_md.char_props.write = read_bit(charac->mode, (BLEMODE_WRITE-1));
    char_md.char_props.notify = read_bit(charac->mode, (BLEMODE_NOTIFY-2));
    char_md.char_props.indicate = read_bit(charac->mode, (BLEMODE_INDICATE-5));
    char_md.p_cccd_md = &cccd_md;

    if(char_md.char_props.notify || char_md.char_props.indicate)
    {
        if(num_chars >= MAX_SUPPORTED_CHARS)
        {
            log_error("Maximum chars # reached: %d", MAX_SUPPORTED_CHARS);
            return BLEST_CHARS_FULL;
        }
    }

    //add new characteristic to the service
    ble_gatts_char_handles_t hnd;
    error = sd_ble_gatts_characteristic_add(charac->hnd_service,
                                            &char_md, &attr_value, &hnd);

    CHECK_STATUS(error, ERRBLINK_BLE_CHARADD);

    num_chars++;
    chars_ctrl[num_chars].hnd_value = hnd.value_handle;
    chars_ctrl[num_chars].hnd_cccd = hnd.cccd_handle;
    chars_ctrl[num_chars].value_cccd = 0;
    chars_ctrl[num_chars].uuid16 = uuid16.uuid;

    *handle = hnd.value_handle;

    return BLEST_OP_OK;
}

BLEStatus hal_ble_add_service(const ServiceData *service, uint16_t *handle)
{
    if(!configured)
        return BLEST_OP_FAIL;

    uint32_t error;
    ble_uuid_t uuid16;
    ble_uuid128_t uuid128;

    uuid16.uuid = service->uuid16;
    memcpy(&uuid128, service->uuid128, 16);

    //add custom UUID
    error = sd_ble_uuid_vs_add(&uuid128, &uuid16.type);
    if(NRF_SUCCESS != error)
    {
    	diag_inc_flag(FLG_BLE_GATT_SERVICE);
        log_error("uuid add: %d", error);
    }
    CHECK_STATUS(error, ERRBLINK_BLE_UUIDADD);

    error = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                         &uuid16, handle);
    if(NRF_SUCCESS != error)
    {
    	diag_inc_flag(FLG_BLE_GATT_SERVICE);
        log_error("gatt srv add: %d", error);
    }
    CHECK_STATUS(error, ERRBLINK_BLE_SERVICEADD);

    return BLEST_OP_OK;
}

BLEStatus hal_ble_set_advdata(uint8_t *data, uint8_t length)
{
    if(MAX_ADV_DATALEN < length)
        return BLEST_LEN_INVALID;
    memset(advdata, 0, MAX_ADV_DATALEN);
    memcpy(advdata, data, length);
    advdata_len = length;
    return BLEST_OP_OK;
}

BLEStatus hal_ble_advertise_start(const uint16_t *uuids, uint16_t len)
{
    if(!configured)
        return BLEST_OP_FAIL;

    uint16_t i;
    uint32_t err_code;
    ble_advdata_t ble_advdata, srdata;
    ble_adv_modes_config_t options = {0};
    ble_uuid_t uuidlist[len];

    //copy uuids
    for(i = 0; i < len; i++)
    {
        uuidlist[i].type = BLE_UUID_TYPE_VENDOR_BEGIN;
        uuidlist[i].uuid = uuids[i];
    }

    update_adv_data();
    // Build advertising data struct
    memset(&ble_advdata, 0, sizeof(ble_advdata));
    ble_advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    ble_advdata.uuids_complete = adv_srv_uuid;
    ble_advdata.p_service_data_array = &srvdata;
    ble_advdata.service_data_count = 1;

    options.ble_adv_fast_enabled  = true;
    options.ble_adv_fast_interval = BLE_GAP_ADV_INTERVAL_MIN;
    options.ble_adv_fast_timeout  = 6000; //6000 == 1 minute

    options.ble_adv_slow_enabled = true;
    options.ble_adv_slow_interval = 1600; //1600*0.625ms
    options.ble_adv_slow_timeout  = 0;

    //create scan response packet and include the list of UUIDs
    memset(&srdata, 0, sizeof(srdata));
    srdata.name_type = BLE_ADVDATA_FULL_NAME;
    srdata.uuids_complete.p_uuids = uuidlist;
    srdata.p_tx_power_level = &txpower;

    ble_advertising_init_t init;
    memset(&init, 0, sizeof(init));
    memcpy(&init.advdata, &ble_advdata, sizeof(ble_advdata));
    memcpy(&init.config, &options, sizeof(options));
    init.evt_handler = on_adv_evt;
    memcpy(&init.srdata, &srdata, sizeof(srdata));

    err_code = ble_advertising_init(&m_advertising, &init);
    if(NRF_SUCCESS == err_code)
    {
        ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
        ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    }
    if(NRF_SUCCESS != err_code)
        return BLEST_OP_FAIL;
    return BLEST_OP_OK;
}

BLEStatus hal_ble_advertise_no_connect(void)
{
    uint32_t errcode;
    ble_gap_adv_params_t adv_params;
    update_adv_data();
    memset(&adv_params, 0, sizeof(ble_gap_adv_params_t));
    m_advertising.adv_params.p_peer_addr = NULL;
    m_advertising.adv_params.filter_policy          = BLE_GAP_ADV_FP_ANY;
    m_advertising.adv_params.interval    = 1636;
    errcode = sd_ble_gap_adv_start(m_advertising.adv_handle, m_advertising.conn_cfg_tag);
    if(NRF_SUCCESS == errcode)
    {
        log_debug("Advertising after connected");
    }
    else
    {
    	diag_inc_flag(FLG_BLE_ADV_NO_CONNECT);
        log_error("Error to advertise when connected: %d", errcode);
    }
    return BLEST_OP_OK;
}

BLEStatus hal_ble_advertise_restart(void)
{
    if(connected)
        return BLEST_OP_FAIL;
    uint32_t errcode;
    ble_gap_adv_params_t adv_params;
    update_adv_data();
    memset(&adv_params, 0, sizeof(ble_gap_adv_params_t));
    m_advertising.adv_params.p_peer_addr = NULL;
    m_advertising.adv_params.filter_policy          = BLE_GAP_ADV_FP_ANY;
    m_advertising.adv_params.interval    = 300;
    errcode = sd_ble_gap_adv_start(m_advertising.adv_handle, m_advertising.conn_cfg_tag);
    if(NRF_SUCCESS == errcode)
    {
        log_debug("Advertising restart ok");
    }
    else
    {
    	diag_inc_flag(FLG_BLE_ADV_RESTART);
        log_error("Error on advertise restart: %d", errcode);
    }
    return BLEST_OP_OK;
}

BLEStatus hal_ble_advertise_stop(void)
{
    uint32_t error;
    BLEStatus status;
    error = sd_ble_gap_adv_stop(m_advertising.adv_handle);
    if(NRF_SUCCESS == error)
        status = BLEST_OP_OK;
    else
    {
        status = BLEST_OP_FAIL;
        diag_inc_flag(FLG_BLE_ADV_STOP);
        log_error("Advertise stop error code: %ld", error);
    }
    return status;
}

BLEStatus hal_ble_connect(void)
{
    if(!configured)
        return BLEST_OP_FAIL;

    uint32_t error;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = APP_TIMER_TICKS(5000);
    cp_init.next_conn_params_update_delay  = APP_TIMER_TICKS(30000);
    cp_init.max_conn_params_update_count   = 0;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_connection_events;
    cp_init.error_handler                  = on_connection_error;

    error = ble_conn_params_init(&cp_init);
    if(NRF_SUCCESS != error)
        return BLEST_OP_FAIL;
    return BLEST_OP_OK;
}

BLEStatus hal_ble_disconnect(void)
{
    uint32_t retcode = ~NRF_SUCCESS;
    if(BLE_CONN_HANDLE_INVALID != conn_handle)
    {
        retcode = sd_ble_gap_disconnect(conn_handle,
                              BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if(NRF_SUCCESS == retcode)
            log_debug("Disconnected by internal call");
        else
        {
        	diag_inc_flag(FLG_BLE_GAP_NOCONN);
        	log_error("Failed to disconnect: %ld", retcode);
        }
    }
    else
    {
        log_error("Not connected. Cannot disconnect");
        diag_inc_flag(FLG_BLE_GAP_NOCONN);
    }
    if(NRF_SUCCESS == retcode)
        return BLEST_OP_OK;
    return BLEST_OP_FAIL;
}

BLEStatus hal_ble_set_module_name(const char *name)
{
    uint32_t err;
    uint16_t len_name = 0;
    ble_gap_conn_sec_mode_t security;

    len_name = str_length(name, BLE_GAP_DEVNAME_MAX_LEN);
    if(0 == len_name)
        return BLEST_LEN_INVALID;

    //set security mode
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&security);
    //store device name and security mode
    err = sd_ble_gap_device_name_set(&security,(const uint8_t*)name, len_name);
    if(NRF_SUCCESS != err)
    {
        log_error("set module name error: %ld, %s, %d", err, name, len_name);
        diag_inc_flag(FLG_BLE_SET_NAME);
        return BLEST_OP_FAIL;
    }
    else
        log_debug("module name set: %s", name);
    return BLEST_OP_OK;
}

BLEStatus hal_ble_get_conn_interval(uint16_t *max, uint16_t *min)
{
    uint32_t status;
    ble_gap_conn_params_t params;
    status = sd_ble_gap_ppcp_get(&params);
    if(NRF_SUCCESS == status)
    {
        *max = params.max_conn_interval;
        *min = params.min_conn_interval;
        return BLEST_OP_OK;
    }
    return BLEST_OP_FAIL;
}

BLEStatus hal_ble_get_max_throughput(uint16_t conn_interval, float *bytesps)
{
    //check connection
    if(!connected || conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        *bytesps = 0;
        return BLEST_OP_OK;
    }
    *bytesps = (MAX_BYTE_PACK*MAX_TX_SLOTS)/((conn_interval*TIME_BASE)/1000.0);
    return BLEST_OP_OK;
}

uint16_t hal_ble_get_conn_handle(void)
{
    return conn_handle;
}

static void pm_evt_handler(pm_evt_t const * p_event)
{
#if BLE_SECURED == 1
    pm_handler_on_pm_evt(p_event);
    pm_handler_flash_clean(p_event);
    //the code below may be needed in the future
    /*switch(p_event->evt_id)
    {
        case PM_EVT_CONN_SEC_SUCCEEDED:
        {
            pm_conn_sec_status_t status;
            // Check if the link is authenticated (meaning at least MITM).
            err_code = pm_conn_sec_status_get(p_event->conn_handle, &status);
            if(NRF_SUCCESS != err_code)
            {
                log_error("[hal_ble] failed to get sec status: %d", err_code);
                diag_inc_flag(FLG_BLE_PM_SEC_FAILED);
            }

            if (status.mitm_protected)
            {
                log_debug("[hal_ble] Link secured. Procedure: %d",
                        p_event->params.conn_sec_succeeded.procedure);
                link_secured = true;
            }
            else
            {
                log_error("[hal_ble] Link is not secure!");
                diag_inc_flag(FLG_BLE_PM_SEC_LINK);
                link_secured = false;
            }
        }
        break;
        case PM_EVT_CONN_SEC_FAILED:
        {
            log_error("[sec failed] src: %d, proc: %d, error: %d",
                    p_event->params.conn_sec_failed.error_src,
                    p_event->params.conn_sec_failed.procedure,
                    p_event->params.conn_sec_failed.error);
            link_secured = false;
            diag_inc_flag(FLG_BLE_PM_SEC_FAILED);
        }
        break;
        case PM_EVT_CONN_SEC_CONFIG_REQ:
        {
            // Reject pairing request from an already bonded peer.
            pm_conn_sec_config_t config = {.allow_repairing = false};
            pm_conn_sec_config_reply(p_event->conn_handle, &config);
        }
        break;
        default:
            break;
    }*/
#endif
}

static void call_listener(BLEEvent id, void *data, uint16_t len)
{
    if(NULL == listener)
        return;
    listener(id, data, len);
}

BLEStatus hal_ble_add_evt_listener(void(*l)(BLEEvent, void *, uint16_t))
{
    if(NULL == l)
        return BLEST_CALLB_INVALID;
    if(NULL != listener && listener != l)
        return BLEST_CALLB_FULL;
    listener = l;
    return BLEST_OP_OK;
}

static void on_connection_events(ble_conn_params_evt_t * evt)
{
    call_listener(BLEEVT_CONNECTION, evt, sizeof(ble_conn_params_evt_t));
}

static void on_connection_error(uint32_t error)
{
    call_listener(BLEEVT_CONNECTION_ERROR, &error, sizeof(uint32_t));
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    call_listener(BLEEVT_ADVERTISE, &ble_adv_evt, sizeof(ble_adv_evt_t));
}

static void ble_events_dispatch(ble_evt_t const *event, void *p_context)
{
    ble_advertising_on_ble_evt(event, &m_advertising);

    if(BLE_GAP_EVT_CONNECTED == event->header.evt_id)
    {
        connected = true;
        conn_handle = event->evt.gap_evt.conn_handle;
    }
    else if(BLE_GAP_EVT_DISCONNECTED == event->header.evt_id)
    {
        connected = false;
        link_secured = false;
        mtu_effective = STD_MTU;
        conn_handle = BLE_CONN_HANDLE_INVALID;
    }
    call_listener(BLEEVT_COMMON, (ble_evt_t *)event, sizeof(ble_evt_t));
}

static void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED)
    {
    	mtu_effective = p_evt->params.att_mtu_effective;
        log_debug("[hal_ble] MTU updated %d", mtu_effective);
    }
}

BLEStatus hal_ble_read_char(uint16_t handle, void *data, uint16_t *len)
{
    if(!configured)
        return BLEST_OP_FAIL;
    uint32_t status;
    ble_gatts_value_t value;
    memset(&value, 0, sizeof(ble_gatts_value_t));

    value.p_value = (uint8_t*)data;
    value.len = *len;

    status = sd_ble_gatts_value_get(conn_handle, handle, &value);
    *len = value.len;
    if(NRF_SUCCESS != status)
        return BLEST_OP_FAIL;
    return BLEST_OP_OK;
}

BLEStatus hal_ble_update_chr(uint16_t hnd, void *data, uint16_t len)
{
    if(!configured)
        return BLEST_OP_FAIL;
    uint32_t error;
    ble_gatts_value_t value;
    memset(&value, 0, sizeof(ble_gatts_value_t));

    //read the actual value
    error = sd_ble_gatts_value_get(conn_handle, hnd, &value);
    if(NRF_SUCCESS != error)
        return BLEST_OP_FAIL;
    //check if value length is equal to characteristic length
    if(value.len != len)
        return BLEST_LEN_INVALID;
    //set the new value
    value.p_value = (uint8_t*)data;
    error = sd_ble_gatts_value_set(conn_handle, hnd, &value);
    if(error != NRF_SUCCESS)
        return BLEST_OP_FAIL;
    return BLEST_OP_OK;
}

BLEStatus hal_ble_reset_all_cccd(void)
{
    uint8_t i;
    char_ctrl_t *chr = chars_ctrl;
    for(i = 0; i < MAX_SUPPORTED_CHARS; i++, chr++)
        chr->value_cccd = 0;
    return BLEST_OP_OK;
}

BLEStatus hal_ble_update_cccd(uint16_t hnd, uint16_t value)
{
    char_ctrl_t *chr = get_cccd_by_hnd(hnd);
    if(NULL == chr)
        return BLEST_HND_INVALID;
    chr->value_cccd = value;
    return BLEST_OP_OK;
}

bool hal_ble_is_link_secure(void)
{
    return link_secured;
}

bool hal_ble_is_push_on_by_char_hnd(uint16_t hnd)
{
    char_ctrl_t *chr = get_cccd_by_charhnd(hnd);
    if(NULL == chr)
        return false;
    uint16_t val = chr->value_cccd & NTF_ENABLED;
    val |= chr->value_cccd & IND_ENABLED;
    return (val != 0);
}

BLEStatus hal_ble_push(CharMode mode, uint16_t hnd, void *data, uint16_t len)
{
    if(!connected)
        return BLEST_NOT_CONNECTED;
    uint8_t pushtype;
    //check mode
    if(BLEMODE_NOTIFY & mode)
        pushtype = BLE_GATT_HVX_NOTIFICATION;
    else if(BLEMODE_INDICATE & mode)
        pushtype = BLE_GATT_HVX_INDICATION;
    else
        return BLEST_OP_FAIL;

    uint32_t error;
    ble_gatts_value_t value;
    memset(&value, 0, sizeof(ble_gatts_value_t));
    //read the actual value
    error = sd_ble_gatts_value_get(conn_handle, hnd, &value);
    if(NRF_SUCCESS != error)
        return BLEST_OP_FAIL;
    //check if value length is valid
    if(len > MAX_LEN_CHAR)
        return BLEST_LEN_INVALID;

    ble_gatts_hvx_params_t push;
    memset(&push, 0, sizeof(push));
    push.handle = hnd;
    push.type = pushtype;
    push.p_len = &len;
    push.p_data = (uint8_t*)data;
    error = sd_ble_gatts_hvx(conn_handle, &push);

    if(NRF_ERROR_INVALID_STATE == error)
        return BLEST_NOT_ENABLED;
    if(NRF_SUCCESS != error)
        return BLEST_NO_TX_BUFFERS;
    if(BLE_ERROR_INVALID_ADV_HANDLE == error)
        return BLEST_NO_TX_BUFFERS;
    return BLEST_OP_OK;
}

#endif //UC_ID == UC_NRF52832
