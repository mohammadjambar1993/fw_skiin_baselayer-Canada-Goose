/*
 * heat_ctrl.c
 *
 * "Play and Go" - 2-BY-2 ALTERNATING MODE
 * Group 1: A + C (Diagonal)
 * Group 2: B + D (Diagonal)
 * Time: 5 Seconds per group
 * Voltage: 5V (Forced for safety with 2 channels active)
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
/* USER CONFIGURATION                         */
/* ========================================== */
#define TARGET_VOLTAGE      5     // 5V is SAFE for 2 channels (12.5W total)
#define SWITCH_DELAY_MS     25000  // 5 Seconds switching time
#define MAX_PCB_TEMP_RAW    8500  // 85C Safety Cutoff

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
static SemaphoreHandle_t mtx_heat_chdata = NULL;
static bool session_running = false;
static bool enable_printing = true;

/* Forward Declarations */
static void tsk_heat_control(void *params);
static void get_auto_ctl_settings(void);
static void read_hardware_channels(void);
static bool is_temperature_safe(void);
static void print_channels_parameters(void);

/* ============================================================= */
/* INIT FUNCTIONS                                                */
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
/* CONTROL TASK (2-BY-2 ALTERNATING MODE)                        */
/* ============================================================= */

static void tsk_heat_control(void *params)
{
    // 1. Wait 3 seconds for power bank stability
    vTaskDelay(pdMS_TO_TICKS(3000)); 

    _info(">> STARTING: 2-BY-2 ALTERNATING MODE (A+C then B+D) @ 5V <<");

    // Start immediately (Play and Go)
    session_running = true;

    while(1)
    {
        // -------------------------------------------------
        // PHASE 1: A + C ON (B + D OFF)
        // -------------------------------------------------
        
        // Safety & Voltage Check
        read_hardware_channels();
        if(!is_temperature_safe()) { 
             for(int i=0;i<4;i++) hw_set_dutycycle(i, 0); 
             vTaskDelay(pdMS_TO_TICKS(5000)); continue; 
        }
        if(hw_get_current_voltage() != TARGET_VOLTAGE) hw_set_voltage(TARGET_VOLTAGE);

        // Apply Phase 1
        hw_set_dutycycle(HEATER_A, 100);
        hw_set_dutycycle(HEATER_B, 0);
        hw_set_dutycycle(HEATER_C, 100);
        hw_set_dutycycle(HEATER_D, 0);

        _info(">> GROUP 1: A + C [ON] | B + D [OFF] <<");
        print_channels_parameters();

        // Wait
        vTaskDelay(pdMS_TO_TICKS(SWITCH_DELAY_MS));

        // -------------------------------------------------
        // PHASE 2: B + D ON (A + C OFF)
        // -------------------------------------------------

        // Safety & Voltage Check
        read_hardware_channels();
        if(!is_temperature_safe()) { 
             for(int i=0;i<4;i++) hw_set_dutycycle(i, 0); 
             vTaskDelay(pdMS_TO_TICKS(5000)); continue; 
        }
        if(hw_get_current_voltage() != TARGET_VOLTAGE) hw_set_voltage(TARGET_VOLTAGE);

        // Apply Phase 2
        hw_set_dutycycle(HEATER_A, 0);
        hw_set_dutycycle(HEATER_B, 100);
        hw_set_dutycycle(HEATER_C, 0);
        hw_set_dutycycle(HEATER_D, 100);

        _info(">> GROUP 2: B + D [ON] | A + C [OFF] <<");
        print_channels_parameters();

        // Wait
        vTaskDelay(pdMS_TO_TICKS(SWITCH_DELAY_MS));
    }
}

/* ============================================================= */
/* HELPER FUNCTIONS                                              */
/* ============================================================= */

app_status_t heat_set_channels(cmd_heat_params_t *params)
{
    session_running = true;
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
        if(pcbtemp > MAX_PCB_TEMP_RAW) {
            _error("PCB OVERHEAT: %d", pcbtemp);
            return false;
        }
    }
    return true;
}

/* ============================================================= */
/* BOILERPLATE / DEBUG                                           */
/* ============================================================= */

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