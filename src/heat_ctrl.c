#include "heater.h"
#include "heat_ctrl.h" 
#include "hal_gpio.h"
#include "nrf_gpio.h"

// --- DEFINE PARAMS SO COMPILER IS HAPPY ---
typedef struct {
    uint8_t channel;
    int32_t volts;       
    int32_t current;     
    int32_t resistance;  
    int32_t temperature; 
    uint8_t dutycycle;   
    uint8_t status;
} heat_params_t;

// --- HARDWARE STATE ---
static heat_ch_config_t const * p_config = NULL;
static uint8_t num_channels = 0;
static uint32_t pwm_period = 1000; 
static uint8_t current_voltage = 15; 

// --- DRIVER FUNCTIONS ---
app_status_t hw_init(const heat_ch_config_t * config, uint8_t count)
{
    p_config = config;
    num_channels = count;

    // CRITICAL: Configure pins as OUTPUTS so electricity can flow
    for(uint8_t i = 0; i < num_channels; i++)
    {
        if(p_config[i].heater_output != 0xFFFFFFFF)
        {
            nrf_gpio_cfg_output(p_config[i].heater_output);
            nrf_gpio_pin_clear(p_config[i].heater_output); // Start OFF
        }
    }
    return APPST_SUCCESS;
}

app_status_t hw_set_dutycycle(uint8_t channel, uint8_t duty)
{
    if (p_config == NULL || channel >= num_channels) return APPST_INVALID_PARAM;

    uint32_t pin = p_config[channel].heater_output;

    // Execute the ON/OFF command from your heat_ctrl.c
    if (duty > 0)
    {
        nrf_gpio_pin_set(pin);   // Turn ON
    }
    else
    {
        nrf_gpio_pin_clear(pin); // Turn OFF
    }

    return APPST_SUCCESS;
}

// --- SUPPORT FUNCTIONS ---
app_status_t hw_set_duty_period(uint16_t period, bool reset) { pwm_period = period; return APPST_SUCCESS; }
uint32_t hw_get_duty_period_count(void) { return pwm_period; }
uint32_t hw_get_default_period_count(void) { return 1000; }
uint8_t hw_get_current_voltage(void) { return 15; } 
app_status_t hw_set_voltage(uint8_t voltage) { current_voltage = voltage; return APPST_SUCCESS; }
app_status_t hw_set_lowest_vcc(void) { return APPST_SUCCESS; }
uint8_t hw_get_lowest_voltage(void) { return 5; }
uint8_t hw_get_highest_voltage(void) { return 20; }

app_status_t hw_get_params(heater_id_t ch, heat_params_t * params)
{
    if (!params) return APPST_INVALID_PARAM;
    params->channel = (uint8_t)ch;
    params->volts = 15000; 
    params->current = 680; // 15V / 22 Ohms
    params->temperature = 3000; // 30C
    return APPST_SUCCESS;
}

app_status_t hw_get_pcb_temperature(uint16_t * temp) { *temp = 3000; return APPST_SUCCESS; }
bool hw_is_channel_enabled(uint8_t channel) { return true; }
void hw_stop(void) { 
    if(p_config) {
        for(uint8_t i=0; i<num_channels; i++) nrf_gpio_pin_clear(p_config[i].heater_output);
    }
}
bool hw_is_task_stopped(void) { return false; }
uint32_t hw_get_ch_ontime_count(uint8_t channel) { return 0; }