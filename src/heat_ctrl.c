#include <string.h>
#include "heat_ctrl.h"
#include "heater.h"
#include "logger.h"
#include "mya_util.h"
#include "algos/autotempctl.h"
#include "semphr.h"
#include "ble_rpc.h"

#define MAX_HEATERS 5
#define MTX_CH_DATA_TOUT 5

#if PCB_ID == PCB_MULTI_CHANNEL
static const heat_ch_config_t channels_config[] = {
    { .id = 0, .heater_output = HEATING_CHA, .addr_ina231 = 0x40, .analog_mux_addr = 0 },
    { .id = 1, .heater_output = HEATING_CHB, .addr_ina231 = 0x41, .analog_mux_addr = 2 },
    { .id = 2, .heater_output = HEATING_CHC, .addr_ina231 = 0x44, .analog_mux_addr = 1 },
    { .id = 3, .heater_output = HEATING_CHD, .addr_ina231 = 0x45, .analog_mux_addr = 3 },
    { .id = 4, .heater_output = HEATING_CHE, .addr_ina231 = 0x42, .analog_mux_addr = 0 },
};
#else
static const heat_ch_config_t channels_config[] = {
    { .id = 0, .heater_output = HEATING_CHA, .addr_ina231 = 0x40, .analog_mux_addr = 0 },
};
#endif

static channel_ctl_t heat_channels[MAX_HEATERS] = {0};
static cmd_heat_params_t last_heat_params;
static bool autoctl = false;
static SemaphoreHandle_t mtx_heat_chdata = NULL;
static TimerHandle_t timer_heat_session = NULL;
static uint16_t session_time_remaining = 0;
static bool session_running = false;

static void tsk_heat_control(void *params);
static void irq_timer_heat_session(TimerHandle_t xtimer);
static void get_auto_ctl_settings(void);

app_status_t heat_init(void)
{
    uint8_t num = sizeof(channels_config)/sizeof(channels_config[0]);
    hw_init(channels_config, num); 
    
    memset(heat_channels, 0, sizeof(heat_channels));
    get_auto_ctl_settings();
    os_create_mutex(&mtx_heat_chdata);
    tctl_init(); 
    
    xTaskCreate(tsk_heat_control, "HeatCtrl", 512, NULL, 3, NULL);
    return APPST_SUCCESS;
}

static void get_auto_ctl_settings(void) {
    for(uint8_t i = 0; i < MAX_HEATERS; i++) band_get_ctl_settings(i, &heat_channels[i].ctl_settings);
}

// Duty Cycle Mode (Used by main.c)
app_status_t heat_set_channels(cmd_heat_params_t *params)
{
    memcpy(&last_heat_params, params, sizeof(cmd_heat_params_t));
    autoctl = false; 
    session_time_remaining = params->timeout_secs;
    session_running = true;
    if(timer_heat_session) xTimerStart(timer_heat_session, 0);
    os_evt_trigger(EVT_HEAT_NEW_SESSION);
    return APPST_SUCCESS;
}

// Temperature Mode (Not used right now, but needed for linker)
app_status_t heat_set_temperature_sp(cmd_heat_params_t *params)
{
    memcpy(&last_heat_params, params, sizeof(cmd_heat_params_t));
    autoctl = true; 
    session_time_remaining = params->timeout_secs;
    session_running = true;
    if(timer_heat_session) xTimerStart(timer_heat_session, 0);
    os_evt_trigger(EVT_HEAT_NEW_SESSION);
    return APPST_SUCCESS;
}

static void tsk_heat_control(void *params)
{
    timer_heat_session = xTimerCreate("tm", pdMS_TO_TICKS(1000), pdTRUE, 0, irq_timer_heat_session);
    
    while(1)
    {
        // Wait for next command from main.c
        os_evt_wait(EVT_HEAT_NEW_SESSION | EVT_HEAT_SESSION_TIMEOUT | EVT_HEAT_SAMPLE, pdMS_TO_TICKS(1000));
        
        // Apply Duty Cycles continuously
        if(!autoctl) 
        {
            for(int i=0; i<MAX_HEATERS; i++) {
                hw_set_dutycycle(i, last_heat_params.data[i]);
            }
        }
    }
}

static void irq_timer_heat_session(TimerHandle_t xtimer) {
    if(session_running && session_time_remaining > 0) session_time_remaining--;
    os_evt_trigger(EVT_HEAT_SESSION_TIMEOUT);
}

// System Stubs
bool heat_get_channel_params(heater_id_t ch, channel_ctl_t *params) { return true; }
bool heat_is_channel_overheating(heater_id_t ch) { return false; }
bool heat_is_hardware_failed(void) { return false; }
uint16_t heat_get_remaining_session_time(void) { return session_time_remaining; }
uint16_t heat_get_remaining_ble_disconnection_time(void) { return 0xFFFF; }
void heat_stop_control(void) { hw_stop(); }
void heat_cycle_printing(void) {}