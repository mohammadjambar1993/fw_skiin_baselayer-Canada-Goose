/*
 * heat_ctrl.c
 *
 * MODIFIED VERSION: 
 * - Toggles A+C / B+D every 20 seconds.
 * - Supports Low/Med/High from App.
 * - Forces Hardware Updates (Fixes A+C bug).
 * - Includes BLE Safety Disconnect.
 */

#include <string.h>
#include "heat_ctrl.h"
#include "heater.h"
#include "logger.h"
#include "mya_util.h"
#include "algos/autotempctl.h"
#include "semphr.h"
#include "ble_rpc.h" // Added for Connection Safety

#define ENABLE_LOGGER_HEATCTRL
#ifdef ENABLE_LOGGER_HEATCTRL
    #include "util/logger.h"
    #define logtag  "[heatctl] "
    #define _debug(...)     log_debug(logtag __VA_ARGS__)
    #define _info(...)      log_info(logtag __VA_ARGS__)
    #define _warn(...)      log_warn(logtag __VA_ARGS__)
    #define _error(...)     log_error(logtag __VA_ARGS__)
    #define _print(...)                                     \
    do                                              \
    {                                               \
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

// --- POWER LEVEL DEFINITIONS ---
// Ensure High is 100 for MAX Power
#define DUTY_LOW    38
#define DUTY_MED    57
#define DUTY_HIGH   100  

#define MAX_TEMPERATURE_SETPOINT    45
#define MAX_PCB_TEMPERATURE         6000 //60C
#define MTX_CH_DATA_TOUT            5

#if PCB_ID == PCB_MULTI_CHANNEL
static const heat_ch_config_t channels_config[] =
{
    {//channel A
        .id = HEATER_A,
        .heater_output = HEATING_CHA,
        .addr_ina231 = 0x40,
        .analog_mux_addr = 0
    },
    {//channel B
        .id = HEATER_B,
        .heater_output = HEATING_CHB,
        .addr_ina231 = 0x41,
        .analog_mux_addr = 2
    },
    {//channel C
        .id = HEATER_C,
        .heater_output = HEATING_CHC,
        .addr_ina231 = 0x44,
        .analog_mux_addr = 1
    },
    {//channel D
        .id = HEATER_D,
        .heater_output = HEATING_CHD,
        .addr_ina231 = 0x45,
        .analog_mux_addr = 3
    },
    {//channel E
        .id = HEATER_E,
        .heater_output = HEATING_CHE,
        .addr_ina231 = 0x42,
        .analog_mux_addr = 0                //Same as channel A
    },
};
#elif PCB_ID == PCB_DUAL_CHANNEL
static const heat_ch_config_t channels_config[] =
{
    {//channel A
        .id = HEATER_A,
        .heater_output = HEATING_CHA,
        .addr_ina231 = 0x40,
        .analog_mux_addr = 0
    },
    //channel B is disabled due to bug in the ina i2c address
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

/* Timer for session timeout*/
static TimerHandle_t timer_heat_session = NULL;
static inline void create_heat_session_timer(void);
static void irq_timer_heat_session(TimerHandle_t xtimer);

static uint16_t session_time_remaining_counter = 0;
static bool session_running = 0;

// NEW: BLE Safety Counter
static uint8_t ble_disconnection_time_remaining_counter = 0xFF;

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
static void ble_disconnection_time_set(void); // NEW

app_status_t heat_init(void)
{
    if(initialized)
        return APPST_INVALID_STATE;
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
        app_status_t st = os_create_mutex(&mtx_heat_chdata);
        if(st == APPST_OS_ERROR)
        {
            _error("Failed to create mutex");
            return APPST_OS_ERROR;
        }
        reset_last_heat_params();
        status = tctl_init();
        if(APPST_SUCCESS == status)
        {
            _error("failed to init temperature control algorithm");
        }
    }
    else
    {
        _error("failed to init heater module: %d", status);
    }
    return status;
}

static channel_ctl_t *get_channel(heater_id_t ch)
{
    if(ch >= MAX_HEATERS)
    {
        _warn("invalid channel: %d", ch);
        return NULL;
    }
    return &heat_channels[ch];
}


bool heat_get_channel_params(heater_id_t ch, channel_ctl_t *params)
{
    if(ch >= MAX_HEATERS || NULL == params)
    {
        _warn("invalid channel/parameters: %d", ch);
        return false;
    }
    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT))
    {
        _warn("Cannot acquire heat_ctl mutex");
        return false;
    }
    memcpy(params, &heat_channels[ch], sizeof(channel_ctl_t));
    os_release_mutx(&mtx_heat_chdata);

    return true;
}

static void get_auto_ctl_settings(void)
{
    app_status_t status;
    heater_id_t ch = HEATER_A;
    channel_ctl_t *ptr = heat_channels;
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++, ptr++)
    {
        status = band_get_ctl_settings(ch, &ptr->ctl_settings);
        if(APPST_SUCCESS != status)
        {
            _error("failed to get channel %d settings: %d", ch, status);
        }
    }
}

static bool is_valid_heat_params(cmd_heat_params_t *params, bool autoctl)
{
    //check duty cycles
    for(uint i = 0; i < MAX_HEATERS; i++)
    {
        if(autoctl) //automatic control. Data field is setpoint
        {
            if(params->data[i] > MAX_TEMPERATURE_SETPOINT)
                return false;
        }
        else //manual control. Data field is duty cycle
        {
            if(params->data[i] > 100)
                return false;
        }
    }
    if(!autoctl)
    {
        //check if voltage is valid
        if(!hw_is_valid_voltage(params->voltage))
        {
            _error("voltage %d not available", params->voltage);
            return false;
        }
    }
    return true;
}

static void shutoff_outputs(void)
{
    heater_id_t ch = HEATER_A;
    //reset all duty cycles
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
    {
        hw_set_dutycycle(ch, 0);
    }
    //reset period
    hw_set_duty_period(hw_get_default_period_count(), true);
    //set lowest possible battery VCC
    hw_set_lowest_vcc();
}

static void restore_last_command(void)
{
    app_status_t status;
    if(autoctl)
        status = heat_set_temperature_sp(&last_heat_params_temp);
    else
        status = heat_set_channels(&last_heat_params_temp);

    if(APPST_SUCCESS != status)
    {
        _warn("failed to restore command");
    }
}

static void save_command(void)
{
    memcpy(&last_heat_params, &last_heat_params_temp, sizeof(cmd_heat_params_t));
    session_time_remaining_counter = last_heat_params_temp.timeout_secs;
    session_running = true;
}

app_status_t heat_set_channels(cmd_heat_params_t *params)
{
    if(!initialized)
        return APPST_INVALID_STATE;
    if(!is_valid_heat_params(params, false))
        return APPST_INVALID_PARAM;
    autoctl = false; //disable automatic control
    //store last command in case safety guard time is activated
    memcpy(&last_heat_params_temp, params, sizeof(cmd_heat_params_t));
    os_evt_trigger(EVT_HEAT_NEW_SESSION);

    return APPST_SUCCESS;
}

app_status_t heat_set_temperature_sp(cmd_heat_params_t *params)
{
    channel_ctl_t *ch = heat_channels;

    if(!initialized)
        return APPST_INVALID_STATE;
    if(!is_valid_heat_params(params, true))
        return APPST_INVALID_PARAM;
    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT))
    {
        _warn("Cannot acquire heat_ctl mutex");
        return APPST_ERROR;
    }
    //read temperature set point for each channel
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
    {
        ch->temperature_setpoint = params->data[i];
        //reset all duty cycles that could be set previously by manual commands
        if(!autoctl)
        {
            hw_set_dutycycle(ch->measures.channel, 0);
        }
    }
    os_release_mutx(&mtx_heat_chdata);
    autoctl = true; //enable automatic control
    //store last command in case safety guard time is activated
    memcpy(&last_heat_params_temp, params, sizeof(cmd_heat_params_t));
    os_evt_trigger(EVT_HEAT_NEW_SESSION);

    return APPST_SUCCESS;
}


bool heat_is_channel_overheating(heater_id_t ch)
{
    channel_ctl_t *ptr = get_channel(ch);
    if(NULL == ptr)
        return false;
    return ptr->overheating;
}

bool heat_is_hardware_failed(void)
{
    return hardware_fail;
}

static void run_safe_guard_time(TickType_t delay)
{
    hardware_fail = true;
    shutoff_outputs();
    _warn("power outputs OFF! Waiting %d ms to resume...", delay);
    os_delay_ms(delay);
    _warn("resuming operation after guard time...");
    restore_last_command();
    hardware_fail = false;
}

static bool is_temperature_safe(void)
{
    const channel_ctl_t *ch = heat_channels;
    app_status_t st;
    uint16_t max_temperature, pcbtemp;

    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT))
    {
        _warn("Cannot acquire heat_ctl mutex");
        return false;
    }
    //check if garment temperature is inside bounds
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
    {
        if(!ch->ctl_settings.enabled)
            continue;
        max_temperature = ch->ctl_settings.temp_cutoff*100;
        if(ch->measures.temperature > max_temperature)
        {
            _warn("ch %d is overheating: %d", ch->measures.temperature);
            return false;
        }
    }
    os_release_mutx(&mtx_heat_chdata);
    //check PCB temperature
    st = hw_get_pcb_temperature(&pcbtemp);
    if(APPST_SUCCESS != st)
    {
        _warn("cannot read PCB temperature: %d", st);
        return false;
    }
    if(pcbtemp > MAX_PCB_TEMPERATURE)
    {
        _warn("PCB overheating: %d", pcbtemp);
        return false;
    }
    return true;
}

static void run_automatic_temperature_control(void)
{
    app_status_t status;
    cmd_heat_params_t params;

    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT))
    {
        _warn("Cannot acquire heat_ctl mutex");
        return;
    }
    //run the algorithm
    status = tctl_run(heat_channels);
    if(APPST_SUCCESS != status)
    {
        _warn("error calling control algorithm: %d", status);
        return;
    }
    os_release_mutx(&mtx_heat_chdata);
    //get the parameters and update the outputs
    status = tctl_get_result(&params);
    if(APPST_SUCCESS != status)
    {
        _warn("cannot get algorithm params: %d", status);
        return;
    }

    update_hw_duties(params.data);
    update_hw_period(params.pwm_period);
    update_hw_voltage(params.voltage);
}

static void read_hardware_channels(void)
{
    heater_id_t id;
    app_status_t status;
    channel_ctl_t *ch = heat_channels;

    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT))
    {
        _warn("Cannot acquire heat_ctl mutex");
        return;
    }
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
    {
        id = ch->ctl_settings.channelid;
        status = hw_get_params(id, &ch->measures);
        if(APPST_SUCCESS != status)
        {
            _warn("fail to read hw channel %d: %d", id, status);
        }
    }
    os_release_mutx(&mtx_heat_chdata);
}

static void update_hw_period(uint32_t newperiod)
{
    app_status_t status;
    uint32_t period = hw_get_duty_period_count();
    if(newperiod != period)
    {
        status = hw_set_duty_period(newperiod, true);
        if(APPST_SUCCESS == status)
        {
            _info("period changed from %d to %d", period, newperiod);
        }
        else
        {
            _warn("failed to set period from %d to %d, %d", period,
                    newperiod, status);
        }
    }
}

static void update_hw_voltage(uint8_t newvoltage)
{
    app_status_t status;
    uint8_t vcc = hw_get_current_voltage();
    if(newvoltage != vcc && hw_is_valid_voltage(newvoltage))
    {
        status = hw_set_voltage(newvoltage);
        if(APPST_SUCCESS == status)
        {
            _info("vcc changed from %d to %d", vcc, newvoltage);
        }
        else
        {
            _warn("failed to set vcc from %d to %d", vcc, newvoltage);
        }
    }
}

// --------------------------------------------------------------------
// ROBUST SYNC: (A+C) vs (B+D)
// FORCE UPDATES: Ensures commands are always sent
// --------------------------------------------------------------------
// --------------------------------------------------------------------
// FORCE GLOBAL SYNC: (A+C) vs (B+D)
// --------------------------------------------------------------------
static void update_hw_duties(uint8_t *duties)
{
    uint8_t i;
    heater_id_t ch;
    app_status_t st;
    channel_ctl_t *ctl = heat_channels;

    // --- 1. GET TARGET POWER (Global) ---
    // Problem: App might send [0, 100, 0, 100].
    // Fix: Find the maximum value and apply it to EVERYONE.
    uint8_t target_power = 0;
    for(i = 0; i < MAX_HEATERS; i++) {
        if (duties[i] != INVALID_DUTY_CYCLE && duties[i] > target_power) {
            target_power = duties[i];
        }
    }

    // --- 2. MASTER CLOCK (20s Switch) ---
    TickType_t now = xTaskGetTickCount();
    uint32_t period_ticks = pdMS_TO_TICKS(50000); 
    uint32_t half_period  = pdMS_TO_TICKS(25000); 
    
    uint32_t phase = (uint32_t)now % period_ticks;
    bool group_AC_active = (phase < half_period); 

    for(i = 0, ch = HEATER_A; i < MAX_HEATERS; i++, ch++, ctl++)
    {
        // --- 3. BYPASS "IS ENABLED" CHECK ---
        // OLD CODE: if(!hw_is_channel_enabled(ch)) continue;
        // NEW CODE: We run regardless (unless channel ID is totally invalid)
        if (ch >= MAX_HEATERS) continue;

        uint8_t final_power = 0;

        // --- 4. APPLY GROUP LOGIC ---
        if (ch == HEATER_A || ch == HEATER_C)
        {
            if (group_AC_active) final_power = target_power;
            else final_power = 0; 
        }
        else if (ch == HEATER_B || ch == HEATER_D)
        {
            if (!group_AC_active) final_power = target_power;
            else final_power = 0; 
        }
        else 
        {
            final_power = target_power;
        }

        // --- 5. FORCE EXECUTION ---
        st = hw_set_dutycycle(ch, final_power);
        
        // Debug output to see if it's trying
        if (final_power > 0 && st != APPST_SUCCESS) {
             _warn("Force Set CH %d Fail: %d", ch, st);
        }
    }
}


static void tsk_heat_control(void *params)
{
    EventBits_t events, triggered;

    events = EVT_HEAT_SAMPLE | EVT_HEAT_SHORT | EVT_HEAT_NEW_SESSION
            | EVT_HEAT_SESSION_TIMEOUT;
    create_heat_session_timer();

    while(1)
    {
        triggered = os_evt_wait(events, pdMS_TO_TICKS(2500));
        os_evt_clear(events);
        read_hardware_channels();
        //check if temperatures are safe
        if(!is_temperature_safe())
        {
            os_evt_trigger(EVT_HARDWARE_FAIL);
            run_safe_guard_time(pdMS_TO_TICKS(50000));
            continue;
        }
        //check short status
        if(EVT_HEAT_SHORT & triggered)
        {
            os_evt_trigger(EVT_HARDWARE_FAIL);
            run_safe_guard_time(pdMS_TO_TICKS(6000));
            continue;
        }

        if(EVT_HEAT_NEW_SESSION & triggered)
        {
            save_command();
            if(pdPASS != xTimerStart(timer_heat_session, 0))
            {
                diag_inc_flag(FLG_HEAT_TIMER_START);
                _warn("Failed to start heat timer");
            }
        }

        if(EVT_HEAT_SESSION_TIMEOUT & triggered)
        {
            if(is_session_active())
            {
                // SAFETY: Stop if phone disconnects
                if(!ble_is_connected())
                {
                    if(ble_disconnection_time_remaining_counter == 0xFF)
                    {
                        ble_disconnection_time_set();
                    }
                    else if(ble_disconnection_time_remaining_counter == 0)
                    {
                        ble_disconnection_time_remaining_counter = 0xFF;
                        session_time_remaining_counter = 0; 
                    }
                    else
                    {
                        ble_disconnection_time_remaining_counter--;
                    }               
                }
                else if(ble_is_connected() && ble_disconnection_time_remaining_counter != 0xFF)
                {               
                    ble_disconnection_time_remaining_counter = 0xFF;        
                }

                if(!session_time_remaining_counter)
                {
                    session_time_remaining_counter = 0;
                    if(pdPASS != xTimerStop(timer_heat_session, 0))
                    {
                        diag_inc_flag(FLG_HEAT_TIMER_START);
                        _warn("Failed to stop heat timer");
                    }
                    autoctl = false;
                    shutoff_outputs();
                    reset_last_heat_params();
                    _warn("Heat activation timeout");
                    session_reset();
                    continue;
                }
                else
                    session_time_remaining_counter--;
            }
        }

        if(EVT_HEAT_SAMPLE & triggered)
        {
            //period and voltage were set by external commands
            if(autoctl) //automatic control
            {
                run_automatic_temperature_control();
            }
            else //manual control
            {
                update_hw_period(last_heat_params.pwm_period);
                update_hw_voltage(last_heat_params.voltage);
                update_hw_duties(last_heat_params.data);
            }
            print_channels_parameters();
        }
    }
}


static void irq_timer_heat_session(TimerHandle_t xtimer)
{
    os_evt_trigger(EVT_HEAT_SESSION_TIMEOUT);
}

static inline void create_heat_session_timer(void)
{
    timer_heat_session = xTimerCreate("tmheat",pdMS_TO_TICKS(1000),pdTRUE,0,
                                     irq_timer_heat_session);
    if(NULL == timer_heat_session)
    {
        diag_inc_flag(FLG_HEAT_TIMER_CREATE);
        _warn("Failed to create heat timer");
        return;
    }
}

static bool is_session_active(void)
{
    return session_running;
}

static void session_reset(void)
{
    session_running = false;
}

static void reset_last_heat_params(void)
{
    memset(&last_heat_params, 0, sizeof(cmd_heat_params_t));
    memset(&last_heat_params.data, INVALID_DUTY_CYCLE, sizeof(last_heat_params.data));
    last_heat_params.pwm_period = hw_get_default_period_count();
    last_heat_params.timeout_secs = 0;
}

uint16_t heat_get_remaining_session_time(void)
{
    return session_time_remaining_counter;
}

// Added for Safety
uint16_t heat_get_remaining_ble_disconnection_time(void)
{
    return ble_disconnection_time_remaining_counter;
}

static void ble_disconnection_time_set(void)
{
    //Timeout default: 60 seconds after phone disconnects
    ble_disconnection_time_remaining_counter = 60;
}


/* this function is experimental and was implemented to stop the control logic
 * and heater module when writing new configuration into ADS. It is assumed that
 * after the ADS is re-configured, the pod will reset itself */
void heat_stop_control(void)
{
    hw_stop();
}

//following functions are used only for debugging
void heat_cycle_printing(void)
{
    enable_printing = !enable_printing;
    if(enable_printing)
        _debug("Heat params printing enabled");
    else
        _debug("Heat params printing disabled");
}

static void print_channels_parameters(void)
{
    if(!enable_printing)
        return;

    channel_ctl_t *channel = heat_channels;

    //print header
    _print("\r\n\r\n");
    _print("CH\t[V]\t[mA]\t[R]\t[C]\t[Duty]\t[St]");
    for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++)
    {
        if(hw_is_channel_enabled(channel->ctl_settings.channelid))
        {
            _print("\r\n%d\t", channel->ctl_settings.channelid);
            _print("%d\t", channel->measures.volts);
            _print("%d\t", channel->measures.current);
            _print("%d\t", channel->measures.resistance);
            _print("%d\t", channel->measures.temperature);
            _print("%d\t", channel->measures.dutycycle);
            _print("%d\t", channel->measures.status);
        }
    }
}