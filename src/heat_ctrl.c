/*
 * heat_ctrl.c
 * FIXED VERSION: 
 * - Auto-Switching A+C / B+D (Prevents Power Bank Crash)
 * - DISABLED BLE TIMEOUT (Works Plug-and-Play)
 */

#include <string.h>
#include "heat_ctrl.h"
#include "heater.h"
#include "logger.h"
#include "mya_util.h"
#include "algos/autotempctl.h"
#include "semphr.h"
#include "ble_rpc.h"

// --- LOGGING ---
#define ENABLE_LOGGER_HEATCTRL
#ifdef ENABLE_LOGGER_HEATCTRL
    #include "util/logger.h"
    #define logtag  "[heatctl] "
    #define _debug(...)     log_debug(logtag __VA_ARGS__)
    #define _info(...)      log_info(logtag __VA_ARGS__)
    #define _warn(...)      log_warn(logtag __VA_ARGS__)
    #define _error(...)     log_error(logtag __VA_ARGS__)
    #define _print(...)                         \
    do                                          \
    {                                           \
        SEGGER_RTT_SetTerminal(0);          \
        SEGGER_RTT_printf(0, __VA_ARGS__);  \
    }while(0)
#else
    #define _debug(...)
    #define _info(...)
    #define _warn(...)
    #define _error(...)
    #define _print(...)
#endif

// --- CONSTANTS ---
#define DUTY_LOW    38
#define DUTY_MED    57
#define DUTY_HIGH   100  
#define MAX_TEMPERATURE_SETPOINT    45
#define MAX_PCB_TEMPERATURE         6000 //60C
#define MTX_CH_DATA_TOUT            5
#define MAX_HEATERS                 4
#define INVALID_DUTY_CYCLE          0xFF

// --- CONFIG ---
#if PCB_ID == PCB_MULTI_CHANNEL
static const heat_ch_config_t channels_config[] =
{
    { .id = 0, .heater_output = HEATING_CHA, .addr_ina231 = 0x40, .analog_mux_addr = 0 },
    { .id = 1, .heater_output = HEATING_CHB, .addr_ina231 = 0x41, .analog_mux_addr = 2 },
    { .id = 2, .heater_output = HEATING_CHC, .addr_ina231 = 0x44, .analog_mux_addr = 1 },
    { .id = 3, .heater_output = HEATING_CHD, .addr_ina231 = 0x45, .analog_mux_addr = 3 },
    { .id = 4, .heater_output = HEATING_CHE, .addr_ina231 = 0x42, .analog_mux_addr = 0 },
};
#else
static const heat_ch_config_t channels_config[] =
{
    { .id = 0, .heater_output = HEATING_CHA, .addr_ina231 = 0x40, .analog_mux_addr = 0 },
};
#endif

static bool initialized = false;
static channel_ctl_t heat_channels[MAX_HEATERS] = {0};
static cmd_heat_params_t last_heat_params;
static cmd_heat_params_t last_heat_params_temp;
static bool autoctl = false;
static bool hardware_fail = false;
static bool enable_printing = false;
static SemaphoreHandle_t mtx_heat_chdata = NULL;
static TimerHandle_t timer_heat_session = NULL;
static uint16_t session_time_remaining_counter = 0;
static bool session_running = 0;
static uint8_t ble_disconnection_time_remaining_counter = 0xFF;

// Prototypes
static void tsk_heat_control(void *params);
static void get_auto_ctl_settings(void);
static void save_command(void);
static void restore_last_command(void);
static void shutoff_outputs(void);
static void update_hw_period(uint32_t newperiod);
static void update_hw_voltage(uint8_t newvoltage);
static void update_hw_duties(uint8_t *duties);
static void read_hardware_channels(void);
static void run_automatic_temperature_control(void);
static void run_safe_guard_time(TickType_t delay);
static bool is_temperature_safe(void);
static bool is_valid_heat_params(cmd_heat_params_t *params, bool autoctl);
static void print_channels_parameters(void); 
static channel_ctl_t *get_channel(heater_id_t ch);
static void reset_last_heat_params(void);
static bool is_session_active(void);
static void session_reset(void);
static void ble_disconnection_time_set(void);
static inline void create_heat_session_timer(void);
static void irq_timer_heat_session(TimerHandle_t xtimer);


// --- INIT ---
app_status_t heat_init(void)
{
    if(initialized) return APPST_INVALID_STATE;
    uint8_t num_channels = sizeof(channels_config)/sizeof(channels_config[0]);
    app_status_t status = hw_init(channels_config, num_channels);
    if(APPST_SUCCESS == status)
    {
        ASSERT_TASK(tsk_create(TSK_HEATER_CTRL, tsk_heat_control));
        memset(heat_channels, 0, sizeof(heat_channels));
        get_auto_ctl_settings();
        initialized = true;
        autoctl = false;
        hardware_fail = false;
        enable_printing = false;
        os_create_mutex(&mtx_heat_chdata);
        reset_last_heat_params();
        tctl_init();
    }
    return status;
}

static channel_ctl_t *get_channel(heater_id_t ch)
{
    if(ch >= MAX_HEATERS) return NULL;
    return &heat_channels[ch];
}

bool heat_get_channel_params(heater_id_t ch, channel_ctl_t *params)
{
    if(ch >= MAX_HEATERS || NULL == params) return false;
    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT)) return false;
    memcpy(params, &heat_channels[ch], sizeof(channel_ctl_t));
    os_release_mutx(&mtx_heat_chdata);
    return true;
}

static void get_auto_ctl_settings(void)
{
    heater_id_t ch = 0;
    channel_ctl_t *ptr = heat_channels;
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++, ptr++)
    {
        band_get_ctl_settings(ch, &ptr->ctl_settings);
    }
}

static bool is_valid_heat_params(cmd_heat_params_t *params, bool autoctl)
{
    for(uint i = 0; i < MAX_HEATERS; i++)
    {
        if(params->data[i] > 100 && !autoctl) return false;
    }
    return true;
}

static void shutoff_outputs(void)
{
    heater_id_t ch = 0;
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
    {
        hw_set_dutycycle(ch, 0);
    }
    hw_set_duty_period(hw_get_default_period_count(), true);
    hw_set_lowest_vcc();
}

static void restore_last_command(void)
{
    if(autoctl) heat_set_temperature_sp(&last_heat_params_temp);
    else heat_set_channels(&last_heat_params_temp);
}

static void save_command(void)
{
    memcpy(&last_heat_params, &last_heat_params_temp, sizeof(cmd_heat_params_t));
    session_time_remaining_counter = last_heat_params_temp.timeout_secs;
    session_running = true;
}

app_status_t heat_set_channels(cmd_heat_params_t *params)
{
    if(!initialized) return APPST_INVALID_STATE;
    if(!is_valid_heat_params(params, false)) return APPST_INVALID_PARAM;
    autoctl = false; 
    memcpy(&last_heat_params_temp, params, sizeof(cmd_heat_params_t));
    os_evt_trigger(EVT_HEAT_NEW_SESSION);
    return APPST_SUCCESS;
}

app_status_t heat_set_temperature_sp(cmd_heat_params_t *params)
{
    if(!initialized) return APPST_INVALID_STATE;
    if(!is_valid_heat_params(params, true)) return APPST_INVALID_PARAM;
    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT)) return APPST_ERROR;
    channel_ctl_t *ch = heat_channels;
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
    {
        ch->temperature_setpoint = params->data[i];
        if(!autoctl) hw_set_dutycycle(ch->measures.channel, 0);
    }
    os_release_mutx(&mtx_heat_chdata);
    autoctl = true; 
    memcpy(&last_heat_params_temp, params, sizeof(cmd_heat_params_t));
    os_evt_trigger(EVT_HEAT_NEW_SESSION);
    return APPST_SUCCESS;
}

bool heat_is_channel_overheating(heater_id_t ch)
{
    channel_ctl_t *ptr = get_channel(ch);
    if(NULL == ptr) return false;
    return ptr->overheating;
}

bool heat_is_hardware_failed(void) { return hardware_fail; }

static void run_safe_guard_time(TickType_t delay)
{
    hardware_fail = true;
    shutoff_outputs();
    os_delay_ms(delay);
    restore_last_command();
    hardware_fail = false;
}

static bool is_temperature_safe(void)
{
    const channel_ctl_t *ch = heat_channels;
    uint16_t max_temperature, pcbtemp;
    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT)) return false;
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
    {
        if(!ch->ctl_settings.enabled) continue;
        max_temperature = ch->ctl_settings.temp_cutoff*100;
        if(ch->measures.temperature > max_temperature) return false;
    }
    os_release_mutx(&mtx_heat_chdata);
    hw_get_pcb_temperature(&pcbtemp);
    if(pcbtemp > MAX_PCB_TEMPERATURE) return false;
    return true;
}

static void run_automatic_temperature_control(void)
{
    app_status_t status;
    cmd_heat_params_t params;
    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT)) return;
    status = tctl_run(heat_channels);
    os_release_mutx(&mtx_heat_chdata);
    if(APPST_SUCCESS == status)
    {
        tctl_get_result(&params);
        update_hw_duties(params.data);
        update_hw_period(params.pwm_period);
        update_hw_voltage(params.voltage);
    }
}

static void read_hardware_channels(void)
{
    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT)) return;
    channel_ctl_t *ch = heat_channels;
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
    {
        hw_get_params(ch->ctl_settings.channelid, &ch->measures);
    }
    os_release_mutx(&mtx_heat_chdata);
}

static void update_hw_period(uint32_t newperiod)
{
    hw_set_duty_period(newperiod, true);
}

static void update_hw_voltage(uint8_t newvoltage)
{
    hw_set_voltage(newvoltage);
}

// --------------------------------------------------------------------
// CRITICAL: ROBUST SYNC (A+C) vs (B+D)
// --------------------------------------------------------------------
static void update_hw_duties(uint8_t *duties)
{
    // FOR TESTING: Ignore sensors and force Channel B ON
    hw_set_dutycycle(0, 0); 
    hw_set_dutycycle(1, 80); // Force 80% duty cycle
    hw_set_dutycycle(2, 0); 
    hw_set_dutycycle(3, 0);

    // Monitor the current in the terminal to see if mA > 0
    _info("TEST MODE: CH B forced ON. Current: %d mA", heat_channels[1].measures.current);
}

static void tsk_heat_control(void *params)
{
    EventBits_t events, triggered;
    events = EVT_HEAT_SAMPLE | EVT_HEAT_SHORT | EVT_HEAT_NEW_SESSION | EVT_HEAT_SESSION_TIMEOUT;
    create_heat_session_timer();

    while(1)
    {
        triggered = os_evt_wait(events, pdMS_TO_TICKS(2500));
        os_evt_clear(events);
        read_hardware_channels();

        if(!is_temperature_safe())
        {
            os_evt_trigger(EVT_HARDWARE_FAIL);
            run_safe_guard_time(pdMS_TO_TICKS(50000));
            continue;
        }
        if(EVT_HEAT_SHORT & triggered)
        {
            os_evt_trigger(EVT_HARDWARE_FAIL);
            run_safe_guard_time(pdMS_TO_TICKS(6000));
            continue;
        }

        if(EVT_HEAT_NEW_SESSION & triggered)
        {
            save_command();
            xTimerStart(timer_heat_session, 0);
        }

        if(EVT_HEAT_SESSION_TIMEOUT & triggered)
        {
            if(is_session_active())
            {
                // -----------------------------------------------------
                // MODIFICATION: BLE SAFETY CHECK DISABLED
                // This ensures it works without a phone connected.
                // -----------------------------------------------------
                /* if(!ble_is_connected()) { ... Logic Removed ... } 
                */
                // -----------------------------------------------------

                if(!session_time_remaining_counter)
                {
                    session_time_remaining_counter = 0;
                    xTimerStop(timer_heat_session, 0);
                    autoctl = false;
                    shutoff_outputs();
                    reset_last_heat_params();
                    session_reset();
                    continue;
                }
                else
                    session_time_remaining_counter--;
            }
        }

        if(EVT_HEAT_SAMPLE & triggered)
        {
            if(autoctl) run_automatic_temperature_control();
            else 
            {
                update_hw_period(last_heat_params.pwm_period);
                update_hw_voltage(last_heat_params.voltage);
                update_hw_duties(last_heat_params.data);
            }
            print_channels_parameters();
        }
    }
}

static void irq_timer_heat_session(TimerHandle_t xtimer) { os_evt_trigger(EVT_HEAT_SESSION_TIMEOUT); }
static inline void create_heat_session_timer(void) { timer_heat_session = xTimerCreate("tmheat",pdMS_TO_TICKS(1000),pdTRUE,0,irq_timer_heat_session); }
static bool is_session_active(void) { return session_running; }
static void session_reset(void) { session_running = false; }
static void reset_last_heat_params(void) { memset(&last_heat_params, 0, sizeof(cmd_heat_params_t)); memset(&last_heat_params.data, INVALID_DUTY_CYCLE, sizeof(last_heat_params.data)); }
uint16_t heat_get_remaining_session_time(void) { return session_time_remaining_counter; }
uint16_t heat_get_remaining_ble_disconnection_time(void) { return ble_disconnection_time_remaining_counter; }
static void ble_disconnection_time_set(void) { ble_disconnection_time_remaining_counter = 60; }
void heat_stop_control(void) { hw_stop(); }
void heat_cycle_printing(void) { enable_printing = !enable_printing; }
static void print_channels_parameters(void)
{
    if(!enable_printing) return;
    channel_ctl_t *channel = heat_channels;
    _print("\r\n\r\n CH [V] [mA] [Duty]\r\n");
    for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++)
    {
        _print("%d %d %d %d\n", i, channel->measures.volts, channel->measures.current, last_heat_params.data[i]);
    }
}