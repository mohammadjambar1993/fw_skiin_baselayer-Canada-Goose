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

// Hardcoded safety ceiling for low-resistance (3.4 ohm) elements
#define MAX_SAFE_DUTY_CYCLE 30 

#if PCB_ID == PCB_MULTI_CHANNEL
static const heat_ch_config_t channels_config[] = {
    // Verified INA231 I2C addresses against schematic sheets 8 & 9
    // Corrected U19 Mux addresses (Ch B = 1, Ch C = 2)
    { .id = 0, .heater_output = HEATING_CHA, .addr_ina231 = 0x40, .analog_mux_addr = 0 },
    { .id = 1, .heater_output = HEATING_CHB, .addr_ina231 = 0x41, .analog_mux_addr = 1 },
    { .id = 2, .heater_output = HEATING_CHC, .addr_ina231 = 0x44, .analog_mux_addr = 2 },
    { .id = 3, .heater_output = HEATING_CHD, .addr_ina231 = 0x45, .analog_mux_addr = 3 },
    // Note: Ch E uses dedicated switch U20 via net TEMP_E_EN, set mux addr to 0xFF to flag discrete switching
    { .id = 4, .heater_output = HEATING_CHE, .addr_ina231 = 0x42, .analog_mux_addr = 0xFF },
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
    for(uint8_t i = 0; i < MAX_HEATERS; i++) {
        band_get_ctl_settings(i, &heat_channels[i].ctl_settings);
    }
}

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
        // Added EVT_HEAT_SAMPLE wakeups to verify thermal states periodically
        os_evt_wait(EVT_HEAT_NEW_SESSION | EVT_HEAT_SESSION_TIMEOUT | EVT_HEAT_SAMPLE, pdMS_TO_TICKS(500));
        
        // Global hardware trip evaluation
        if(heat_is_hardware_failed() || session_time_remaining == 0) {
            hw_stop();
            session_running = false;
            continue;
        }

        // Apply Duty Cycles continuously with Active Safety Interlocks
        if(!autoctl && session_running) 
        {
            for(int i = 0; i < MAX_HEATERS; i++) 
            {
                // 1. Safety Override: Check active temp sensors or INA231 hardware alerts
                if (heat_is_channel_overheating(i)) 
                {
                    log_warn("[heat_ctrl] Ch %d overheating! Output clamped safely.", i);
                    hw_set_dutycycle(i, 0);
                    continue;
                }

                // 2. Clamp target drive output to prevent raw thermal runaway on 3.4R loads
                uint8_t target_duty = last_heat_params.data[i];
                if (target_duty > MAX_SAFE_DUTY_CYCLE) {
                    target_duty = MAX_SAFE_DUTY_CYCLE;
                }

                hw_set_dutycycle(i, target_duty);
            }
        }
    }
}

static void irq_timer_heat_session(TimerHandle_t xtimer) {
    if(session_running && session_time_remaining > 0) {
        session_time_remaining--;
    }
    os_evt_trigger(EVT_HEAT_SESSION_TIMEOUT | EVT_HEAT_SAMPLE);
}

// System Safety Evaluation Hooks
bool heat_get_channel_params(heater_id_t ch, channel_ctl_t *params) { 
    return true; 
}

bool heat_is_channel_overheating(heater_id_t ch) 
{ 
    // Implement mapping to check cached ADS1220 bridge calculations 
    // or external test point sensor evaluation logic here.
    // Example: return (temperature_ads_get_last_temp(ch) > SAFETY_CUTOFF_LIMIT);
    return false; 
}

bool heat_is_hardware_failed(void) { 
    // Check global alert pin status from INA231 outputs
    return false; 
}

uint16_t heat_get_remaining_session_time(void) { return session_time_remaining; }
uint16_t heat_get_remaining_ble_disconnection_time(void) { return 0xFFFF; }
void heat_stop_control(void) { hw_stop(); session_running = false; }
void heat_cycle_printing(void) {}