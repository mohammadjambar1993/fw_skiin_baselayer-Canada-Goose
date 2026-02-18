/*
 * heat_ctrl.c
 *
 * "Play and Go" - SEQUENCER MODE
 * Cycle A -> B -> C -> D (10 seconds each)
 * Max Power (15V) to 45C
 *
 */

#include <string.h>
#include "heat_ctrl.h"
#include "heater.h"
#include "logger.h"
#include "mya_util.h"
#include "algos/autotempctl.h"
#include "ble_rpc.h"
#include "semphr.h"

/* ========================================== */
/* USER CONFIGURATION               */
/* ========================================== */
#define TARGET_TEMP_RAW     4500  // 45.00 Degrees Celsius
#define MAX_PCB_TEMP_RAW    8500  // 85.00 Degrees Celsius
#define TARGET_VOLTAGE      15    // Force 15 Volts
#define SECONDS_PER_CH      10    // Time per channel

/* ========================================== */

#define ENABLE_LOGGER_HEATCTRL
#ifdef ENABLE_LOGGER_HEATCTRL
    #include "util/logger.h"
    #define logtag  "[heatctl] "
    #define _debug(...)     log_debug(logtag __VA_ARGS__)
    #define _info(...)      log_info(logtag __VA_ARGS__)
    #define _warn(...)      log_warn(logtag __VA_ARGS__)
    #define _error(...)     log_error(logtag __VA_ARGS__)
    #define _print(...)                                 \
    do                                                  \
    {                                                   \
        SEGGER_RTT_SetTerminal(0);                      \
        SEGGER_RTT_printf(0, __VA_ARGS__);  \
    }while(0)
#else
    #define _debug(...)
    #define _info(...)
    #define _warn(...)
    #define _error(...)
    #define _print(...)
#endif

#define MTX_CH_DATA_TOUT            5

// Channel definitions
#define HEATER_A            0
#define HEATER_B            1
#define HEATER_C            2
#define HEATER_D            3
#define NUM_HEATERS         4

#define INVALID_DUTY_CYCLE  0xFF

/* ----------------------------------------------------------- */
/* FORCED 4 CHANNELS CONFIGURATION                             */
/* ----------------------------------------------------------- */
static const heat_ch_config_t channels_config[] =
{
    { .id = HEATER_A, .heater_output = HEATING_CHA, .addr_ina231 = 0x40, .analog_mux_addr = 0 },
    { .id = HEATER_B, .heater_output = HEATING_CHB, .addr_ina231 = 0x41, .analog_mux_addr = 2 },
    { .id = HEATER_C, .heater_output = HEATING_CHC, .addr_ina231 = 0x44, .analog_mux_addr = 1 },
    { .id = HEATER_D, .heater_output = HEATING_CHD, .addr_ina231 = 0x45, .analog_mux_addr = 3 }
};

static bool initialized = false;
static channel_ctl_t heat_channels[MAX_HEATERS] = {0};
static cmd_heat_params_t last_heat_params;
static SemaphoreHandle_t mtx_heat_chdata = NULL;
static TimerHandle_t timer_heat_session = NULL;
static bool session_running = false;
static bool enable_printing = true;

/* Forward Declarations */
static void tsk_heat_control(void *params);
static void get_auto_ctl_settings(void);
static void read_hardware_channels(void);
static bool is_temperature_safe(void);
static void print_channels_parameters(void);
static void irq_timer_heat_session(TimerHandle_t xtimer);
static inline void create_heat_session_timer(void);

/* ============================================================= */
/* INIT FUNCTIONS                              */
/* ============================================================= */

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
        os_create_mutex(&mtx_heat_chdata);
        tctl_init(); 
    }
    else
    {
        _error("failed to init heater module: %d", status);
    }
    return status;
}

static void get_auto_ctl_settings(void)
{
    heater_id_t ch = HEATER_A;
    channel_ctl_t *ptr = heat_channels;
    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++, ptr++)
    {
        band_get_ctl_settings(ch, &ptr->ctl_settings);
    }
}

/* ============================================================= */
/* CONTROL TASK (SEQUENCER MODE)                */
/* ============================================================= */

static void tsk_heat_control(void *params)
{
    EventBits_t events, triggered;
    events = EVT_HEAT_SAMPLE | EVT_HEAT_SHORT | EVT_HEAT_NEW_SESSION | EVT_HEAT_SESSION_TIMEOUT;
    
    create_heat_session_timer();

    _info(">>> HEAT SEQUENCER STARTED: A->B->C->D (10s each) @ 15V <<<");

    uint32_t seconds_counter = 0;
    uint8_t active_channel = 0;

    while(1)
    {
        // Wait 1000ms (1 second)
        triggered = os_evt_wait(events, pdMS_TO_TICKS(1000)); 
        os_evt_clear(events);

        // 1. Read Sensors
        read_hardware_channels();

        // 2. Safety Check
        if(!is_temperature_safe())
        {
             for(uint8_t i = 0; i < NUM_HEATERS; i++) hw_set_dutycycle(i, 0); 
             _error("SAFETY SHUTDOWN: PCB TOO HOT");
             os_delay_ms(5000);
             continue;
        }

        // 3. Force 15V
        uint8_t current_voltage = hw_get_current_voltage();
        if (current_voltage != TARGET_VOLTAGE) 
        {
             _warn("Setting 15V...");
             hw_set_voltage(TARGET_VOLTAGE); 
        }

        // 4. SEQUENCER LOGIC
        if (session_running) 
        {
            // Calculate which channel should be active based on time
            // Cycle: 0-9s (A), 10-19s (B), 20-29s (C), 30-39s (D)
            uint32_t cycle_time = seconds_counter % (SECONDS_PER_CH * 4);
            
            if (cycle_time < SECONDS_PER_CH) 
                active_channel = HEATER_A;
            else if (cycle_time < (SECONDS_PER_CH * 2)) 
                active_channel = HEATER_B;
            else if (cycle_time < (SECONDS_PER_CH * 3)) 
                active_channel = HEATER_C;
            else 
                active_channel = HEATER_D;

            _info("Time: %ds | Active Channel: %d", seconds_counter, active_channel);

            // Apply logic to all channels
            for(uint8_t i = 0; i < 4; i++)
            {
                if(i == active_channel)
                {
                    // This is the active channel for this 10s slot
                    channel_ctl_t *ch = &heat_channels[i];
                    if (ch->measures.temperature < TARGET_TEMP_RAW)
                        hw_set_dutycycle(i, 100); // Max Power
                    else
                        hw_set_dutycycle(i, 1);   // Maintenance
                }
                else
                {
                    // Inactive channels are OFF
                    hw_set_dutycycle(i, 0);
                }
            }

            // Increment time
            seconds_counter++;
        }
        else
        {
            for(uint8_t i = 0; i < NUM_HEATERS; i++) hw_set_dutycycle(i, 0);
        }

        // 5. Handle Start Event
        if(EVT_HEAT_NEW_SESSION & triggered)
        {
            _info("SESSION START RECEIVED");
            session_running = true;
            seconds_counter = 0; // Reset sequence
            xTimerStart(timer_heat_session, 0);
        }

        // 6. Print Debug
        print_channels_parameters();
    }
}

/* ============================================================= */
/* HELPER FUNCTIONS                            */
/* ============================================================= */

app_status_t heat_set_channels(cmd_heat_params_t *params)
{
    session_running = true;
    os_evt_trigger(EVT_HEAT_NEW_SESSION);
    return APPST_SUCCESS;
}

app_status_t heat_set_temperature_sp(cmd_heat_params_t *params)
{
    return heat_set_channels(params);
}

static void read_hardware_channels(void)
{
    heater_id_t id;
    channel_ctl_t *ch = heat_channels;
    
    if(!os_get_mutex(&mtx_heat_chdata, MTX_CH_DATA_TOUT)) return;

    for(uint8_t i = 0; i < MAX_HEATERS; i++, ch++)
    {
        id = ch->ctl_settings.channelid;
        hw_get_params(id, &ch->measures);
    }
    os_release_mutx(&mtx_heat_chdata);
}

static bool is_temperature_safe(void)
{
    uint16_t pcbtemp;
    if(APPST_SUCCESS == hw_get_pcb_temperature(&pcbtemp))
    {
        if(pcbtemp > MAX_PCB_TEMP_RAW) return false;
    }
    return true;
}

/* ============================================================= */
/* BOILERPLATE / DEBUG                         */
/* ============================================================= */

static void irq_timer_heat_session(TimerHandle_t xtimer)
{
    os_evt_trigger(EVT_HEAT_SESSION_TIMEOUT);
}

static inline void create_heat_session_timer(void)
{
    timer_heat_session = xTimerCreate("tmheat", pdMS_TO_TICKS(10000), pdTRUE, 0, irq_timer_heat_session);
}

bool heat_is_channel_overheating(heater_id_t ch) { return false; }
bool heat_is_hardware_failed(void) { return false; }
uint16_t heat_get_remaining_session_time(void) { return 9999; }
uint16_t heat_get_remaining_ble_disconnection_time(void) { return 9999; }
void heat_stop_control(void) { hw_stop(); }
void heat_cycle_printing(void) { enable_printing = !enable_printing; }
bool heat_get_channel_params(heater_id_t ch, channel_ctl_t *params) { return false; }

static void print_channels_parameters(void)
{
    if(!enable_printing) return;
    channel_ctl_t *channel = heat_channels;
    _print("\nCH  [V]   [mA]   [R]   [Temp]  [St]\n");
    for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++)
    {
        if(hw_is_channel_enabled(channel->ctl_settings.channelid))
        {
            _print("%-4d %-6d %-6d %-6d %-6d %d\n", 
                channel->ctl_settings.channelid, 
                channel->measures.volts,
                channel->measures.current,
                channel->measures.resistance,
                channel->measures.temperature,
                channel->measures.status);
        }
    }
}