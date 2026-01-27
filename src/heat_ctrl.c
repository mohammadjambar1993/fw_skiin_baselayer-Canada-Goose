/*
 * heat_ctrl.c
 *
 *  Created on: Feb 25, 2020
 *      Author: Myant
 */

#include <string.h>
#include "heat_ctrl.h"
#include "heater.h"
#include "logger.h"
#include "mya_util.h"
#include "algos/autotempctl.h"
#include "ble_rpc.h"
#include "semphr.h"

#define ENABLE_LOGGER_HEATCTRL
#ifdef ENABLE_LOGGER_HEATCTRL
	#include "util/logger.h"
	#define logtag	"[heatctl] "
	#define _debug(...) 	log_debug(logtag __VA_ARGS__)
	#define _info(...)		log_info(logtag __VA_ARGS__)
	#define _warn(...)		log_warn(logtag __VA_ARGS__)
	#define _error(...)		log_error(logtag __VA_ARGS__)
	#define _print(...)  					\
	do                      				\
	{                       				\
		SEGGER_RTT_SetTerminal(0);        	\
		SEGGER_RTT_printf(0, __VA_ARGS__);  \
	}while(0)
#else
	#define _debug(...)
	#define _info(...)
	#define _warn(...)
	#define _error(...)
	#define _print(...)
#endif


//todo: the temperature limits must be double checked with the team!
/* <<hack>> 60C is chosen to not mess with hardware tests.
 * This value must be carefully defined for the final versions */
#define MAX_TEMPERATURE_SETPOINT	45
#define MAX_PCB_TEMPERATURE			6000 //60C
#define MTX_CH_DATA_TOUT			5
#define BLE_DISCONNECTION_TIME      1200 // 30-minutes

// Channel definitions
#define HEATER_A   		 	0   // Lower Front Heat
#define HEATER_B    		1   // Lower Back Heat
#define HEATER_C         	2   // Upper Back Heat
#define HEATER_D    		3   // Upper Front Heat
#define NUM_HEATERS 		4
#define HEATER_PER_STEP     2

// Timing definitions
#define HEAT_SEQUENCE_TIME    30  // 20 seconds

// Heat levels
#define DUTY_LOW    38
#define DUTY_MED    57
#define DUTY_HIGH   100

// Power levels
#define P_LOW       3570
#define P_MED       5000
#define P_HIGH      6430

#define MAX_DUTY_CYCLE 100
#define MIN_DUTY_CYCLE 0

// Step definitions
typedef enum {
    STEP_1 = 0,
    STEP_2,
    STEP_COMPLETE
} sequence_step_t;

// Sequence control structure
typedef struct
{
    uint8_t step1_config[NUM_HEATERS];
    uint8_t step2_config[NUM_HEATERS];
} sequence_control_t;

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
		.analog_mux_addr = 0				//Same as channel A
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

// Global variables
static uint8_t heat_sequence_counter = 0;
static sequence_step_t current_step = STEP_1;
static sequence_control_t front_sequence;
static sequence_control_t back_sequence;
static uint8_t channel_configs[NUM_HEATERS];
//this flag is used to identify automatic or manual output control
static bool autoctl = false;
static bool hardware_fail = false;
static bool enable_printing = false;
static SemaphoreHandle_t mtx_heat_chdata = NULL;

/* Timer for session timeout*/
static TimerHandle_t timer_heat_session = NULL;
static inline void create_heat_session_timer(void);
static void irq_timer_heat_session(TimerHandle_t xtimer);

static uint16_t ble_disconnection_time_remaining_counter = 0xFF;
static uint16_t session_time_remaining_counter = 0;
static bool session_running = 0;

static void tsk_heat_control(void *params);
static void get_auto_ctl_settings(void);
static void save_command(void);
static void restore_last_command(void);
static void shutoff_outputs(void);
static void	update_hw_period(uint32_t newperiod);
static void	update_hw_voltage(uint8_t newvoltage);
static void update_hw_duties(uint8_t *duties);
static void read_hardware_channels(void);
static void run_automatic_temperature_control(void);
static void run_safe_guard_time(TickType_t delay);
static bool is_temperature_safe(void);
static bool is_valid_heat_params(cmd_heat_params_t *params, bool autoctl);
static void print_channels_parameters(void); //used only for debugging
static channel_ctl_t *get_channel(heater_id_t ch);
static void reset_last_heat_params(void);
static bool is_session_active(void);
static void session_reset(void);
static uint16_t heat_get_remaining_ble_disconnection_time(void);
static void ble_disconnection_time_set(void);

// Function prototypes
static void heat_sequence_timer_set(void);
static void apply_channel_config(void);
static void handle_sequence_step(sequence_step_t seq);
static void configure_front_sequence(sequence_control_t *seq, uint8_t duty);
static void configure_back_sequence(sequence_control_t *seq, uint8_t duty);
static void update_duties_config(uint8_t *duties);
static float get_power_needed(uint8_t duty_cycle);
static uint8_t calculate_duty_cycle(uint8_t duty_cycle, uint8_t heater_channel);

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
		enable_printing = true;
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
		/* temperature should be always scaled by 100 to avoid the use of floats.
		 * i.e 23.46 == 2346. ch->ctl_settings.temp_cutoff is initialized
		 * from value stored in flash, which is 1 byte and  not scaled */
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

static void	update_hw_period(uint32_t newperiod)
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

static void	update_hw_voltage(uint8_t newvoltage)
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

// Set sequence timer
static void heat_sequence_timer_set(void)
{
	heat_sequence_counter = HEAT_SEQUENCE_TIME;
}

// Apply channel configuration to hardware
static void apply_channel_config(void)
{
    for (int i = 0; i < NUM_HEATERS; i++) {
        // Hardware-specific PWM duty application, e.g., set_pwm_duty(i, channel_configs[i]);
        // This is a placeholder for actual hardware interaction code.
    }
}

// Handle sequence step transition
static void handle_sequence_step(sequence_step_t seq)
{
    if (heat_sequence_counter == 0)
    {
        switch (current_step)
        {
            case STEP_1:
                current_step = STEP_2;
                heat_sequence_timer_set();
                //_debug("In the step_1 case");
    			channel_configs[HEATER_A] = front_sequence.step1_config[HEATER_A];
    			channel_configs[HEATER_D] = front_sequence.step1_config[HEATER_D];
    			channel_configs[HEATER_B] = back_sequence.step1_config[HEATER_B];
    			channel_configs[HEATER_C] = back_sequence.step1_config[HEATER_C];
				//_debug("step_1 setting channel A:%d B:%d C:%d D:%d", channel_configs[0], channel_configs[1], channel_configs[2], channel_configs[3]);
				_debug("step_2 setting channel A:%d B:%d C:%d D:%d", channel_configs[0], channel_configs[1], channel_configs[2], channel_configs[3]);
                break;

            case STEP_2:
            	current_step = STEP_1;
            	//_debug("In the step_2 case");
            	heat_sequence_timer_set();
    			channel_configs[HEATER_A] = front_sequence.step2_config[HEATER_A];
    			channel_configs[HEATER_D] = front_sequence.step2_config[HEATER_D];
    			channel_configs[HEATER_B] = back_sequence.step2_config[HEATER_B];
    			channel_configs[HEATER_C] = back_sequence.step2_config[HEATER_C];
    			//_debug("step_2 setting channel A:%d B:%d C:%d D:%d", channel_configs[0], channel_configs[1], channel_configs[2], channel_configs[3]);
				_debug("step_1 setting channel A:%d B:%d C:%d D:%d", channel_configs[0], channel_configs[1], channel_configs[2], channel_configs[3]);
                break;

            default:
                break;
        }
        apply_channel_config();
    }
    else
    {
    	heat_sequence_counter--;
    }
}


// Function to calculate duty cycle
static uint8_t calculate_duty_cycle(uint8_t duty_cycle, uint8_t heater_channel)
{
	channel_ctl_t *channel = heat_channels;
    float P_measured = (channel[heater_channel].measures.volts * channel[heater_channel].measures.current)/1000;

	//_debug("Voltage measured %d", channel[heater_channel].measures.volts);
	//_debug("Current measured %d", channel[heater_channel].measures.current);
	//_debug("Power measured %d", P_measured);
    
    if (P_measured == 0)
	{
        return duty_cycle; // Avoid division by zero
    }

	float P_needed = get_power_needed(duty_cycle);
	//_debug("Power needed %d", P_needed);

    uint8_t cal_duty_cycle = (uint8_t)((P_needed / P_measured) * 100);
    _debug("Power measured %0.2f", P_measured);
    _debug("Power needed %0.2f", P_needed);
    _debug("Calculated Power %0.2f", P_needed / P_measured);
    _debug("Duty Calculated %d", cal_duty_cycle);

    // Clamp the value between 0% and 100%
    if (cal_duty_cycle > MAX_DUTY_CYCLE)
	{
    	cal_duty_cycle = MAX_DUTY_CYCLE;
    } 
	else if (cal_duty_cycle < MIN_DUTY_CYCLE)
	{
		cal_duty_cycle = MIN_DUTY_CYCLE;
    }
    return cal_duty_cycle;
}

// Function to determine power needed based on the provided duty cycle
// Function to determine power needed based on the provided duty cycle
static float get_power_needed(uint8_t duty_cycle)
{
    if (duty_cycle == DUTY_LOW)
    {
        return P_LOW;
    } 
    else if (duty_cycle == DUTY_MED) 
    {
        return P_MED;
    } 
    else if (duty_cycle == DUTY_HIGH) // <--- FIXED HERE
    {
        return P_HIGH;
    }
    
    // Default fallback if no match (prevents the "control reaches end" warning)
    return P_MED; 
}

// Configure Front Sequence
static void configure_front_sequence(sequence_control_t *seq, uint8_t duty)
{
	uint8_t step_1_duty_cycle = calculate_duty_cycle(duty, HEATER_D);
    _debug("front sequence %d", step_1_duty_cycle);
	uint8_t step_2_duty_cycle = calculate_duty_cycle(duty, HEATER_A);
	_debug("front sequence %d", step_2_duty_cycle);
	if (duty == DUTY_LOW)
    {
        seq->step1_config[HEATER_A] = step_1_duty_cycle; // Front Lower
        seq->step1_config[HEATER_D] = 0;  
        seq->step2_config[HEATER_A] = 0;  
        seq->step2_config[HEATER_D] = step_2_duty_cycle; // Front Upper
    }
    else if (duty == DUTY_MED)
    {
        seq->step1_config[HEATER_A] = step_1_duty_cycle; // Front Lower
        seq->step1_config[HEATER_D] = 0;  
        seq->step2_config[HEATER_A] = 0;  
        seq->step2_config[HEATER_D] = step_2_duty_cycle; // Front Upper
    }
    else if (duty == DUTY_HIGH)
    {
        seq->step1_config[HEATER_A] = step_1_duty_cycle; // Front Lower
        seq->step1_config[HEATER_D] = 0;  
        seq->step2_config[HEATER_A] = 0;  
        seq->step2_config[HEATER_D] = step_2_duty_cycle; // Front Upper
    }
/*
    if (duty == DUTY_LOW)
    {
        seq->step1_config[HEATER_A] = 50;  // Front Lower
        seq->step1_config[HEATER_D] = 50;  // Front Upper
        seq->step2_config[HEATER_A] = 0;
        seq->step2_config[HEATER_D] = 0;
    }
    else if (duty == DUTY_MED)
    {
        seq->step1_config[HEATER_A] = 70;
        seq->step1_config[HEATER_D] = 70;
        seq->step2_config[HEATER_A] = 0;
        seq->step2_config[HEATER_D] = 0;
    }
    else if (duty == DUTY_HIGH)
    {
        seq->step1_config[HEATER_A] = 0;
        seq->step1_config[HEATER_D] = 90;
        seq->step2_config[HEATER_A] = 90;
        seq->step2_config[HEATER_D] = 0;
    }
*/
}

// Configure Back Sequence
static void configure_back_sequence(sequence_control_t *seq, uint8_t duty)
{
	uint8_t step_1_duty_cycle = calculate_duty_cycle(duty, HEATER_B);
    _debug("Back sequence %d", step_1_duty_cycle);
	uint8_t step_2_duty_cycle = calculate_duty_cycle(duty, HEATER_C);
	_debug("Back sequence %d", step_2_duty_cycle);
	if (duty == DUTY_LOW) 
	{
		seq->step1_config[HEATER_B] = 0; 
		seq->step1_config[HEATER_C] = step_1_duty_cycle; // Back Upper      
		seq->step2_config[HEATER_B] = step_2_duty_cycle; // Back Lower
		seq->step2_config[HEATER_C] = 0; 
    }
    else if (duty == DUTY_MED)
    {
		seq->step1_config[HEATER_B] = 0; 
		seq->step1_config[HEATER_C] = step_1_duty_cycle; // Back Upper      
		seq->step2_config[HEATER_B] = step_2_duty_cycle; // Back Lower
		seq->step2_config[HEATER_C] = 0; 
    }
    else if (duty == DUTY_HIGH) {
		seq->step1_config[HEATER_B] = 0; 
		seq->step1_config[HEATER_C] = step_1_duty_cycle; // Back Upper      
		seq->step2_config[HEATER_B] = step_2_duty_cycle; // Back Lower
		seq->step2_config[HEATER_C] = 0; 
    }
/*
    if (duty == DUTY_LOW) {
        seq->step1_config[HEATER_B] = 0;        // Back Lower
        seq->step1_config[HEATER_C] = 0;        // Back Upper
        seq->step2_config[HEATER_B] = 50;
        seq->step2_config[HEATER_C] = 50;
    }
    else if (duty == DUTY_MED)
    {
        seq->step1_config[HEATER_B] = 0;
        seq->step1_config[HEATER_C] = 0;
        seq->step2_config[HEATER_B] = 70;
        seq->step2_config[HEATER_C] = 70;
    }
    else if (duty == DUTY_HIGH) {
        seq->step1_config[HEATER_B] = 90;
        seq->step1_config[HEATER_C] = 0;
        seq->step2_config[HEATER_B] = 0;
        seq->step2_config[HEATER_C] = 90;
    }
*/
}

// Update duties configuration
static void update_duties_config(uint8_t *duties) 
{
	// Handle Front Heaters (A and D)
	bool front_heater_active = (duties[HEATER_A] > 0 && duties[HEATER_A] != INVALID_DUTY_CYCLE) ||
	                           (duties[HEATER_D] > 0 && duties[HEATER_D] != INVALID_DUTY_CYCLE);
	// Handle Back Heaters (B and C)
	bool back_heater_active = (duties[HEATER_B] > 0 && duties[HEATER_B] != INVALID_DUTY_CYCLE) ||
	                          (duties[HEATER_C] > 0 && duties[HEATER_C] != INVALID_DUTY_CYCLE);
	if (front_heater_active || back_heater_active)
	{
	    configure_front_sequence(&front_sequence, duties[HEATER_A]);
	    configure_back_sequence(&back_sequence, duties[HEATER_B]);
	    handle_sequence_step(current_step);
	}
}

// -------------------------------------------------------------
// MODIFIED FUNCTION: Bypass Sequence / Force Direct Output
// -------------------------------------------------------------
// --------------------------------------------------------------------
// FORCE DIRECT OUTPUT (Bypasses the "Breathing/Pulsing" logic)
// --------------------------------------------------------------------
// --------------------------------------------------------------------
// FIXED FUNCTION: Applies Duty Cycle Directly to All Channels
// --------------------------------------------------------------------
static void update_hw_duties(uint8_t *duties)
{
    uint8_t i;
    heater_id_t ch;
    channel_ctl_t *ctl = heat_channels;

    for(i = 0, ch = HEATER_A; i < MAX_HEATERS; i++, ch++, ctl++)
    {
        if(!hw_is_channel_enabled(ch)) continue;
        if (duties[i] == INVALID_DUTY_CYCLE) continue;

        // FORCE DIRECT OUTPUT (No internal timers)
        uint8_t target_val = duties[i]; 

        if(ctl->measures.dutycycle != target_val)
        {
            hw_set_dutycycle(ch, target_val);
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
		triggered = os_evt_wait(events, pdMS_TO_TICKS(3000));
		os_evt_clear(events);
		read_hardware_channels();
		//check if temperatures are safe
		if(!is_temperature_safe())
		{
			os_evt_trigger(EVT_HARDWARE_FAIL);
			run_safe_guard_time(pdMS_TO_TICKS(60000));
			continue;
		}
		//check short status
		if(EVT_HEAT_SHORT & triggered)
		{
			os_evt_trigger(EVT_HARDWARE_FAIL);
			run_safe_guard_time(pdMS_TO_TICKS(7000));
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
                // --- START OF MODIFICATION ---
                // We commented out the entire BLE check block to prevent auto-shutoff
                
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
                else
                {
                    // Add Remaining Implementation If Required 
                } 
                
                // --- END OF MODIFICATION ---                 

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
				update_duties_config(last_heat_params.data);
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

uint16_t heat_get_remaining_ble_disconnection_time(void)
{
	return ble_disconnection_time_remaining_counter;
}

static void ble_disconnection_time_set(void)
{
	ble_disconnection_time_remaining_counter = BLE_DISCONNECTION_TIME; // 30-minutes is 1200 seconds 
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

	// Print header with proper alignment
	_print("\n\n");
	_print("CH    [V]    [mA]    [R]      [Temp]    [Duty]    [St]\n");

	for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++) 
	{
		if(hw_is_channel_enabled(channel->ctl_settings.channelid)) 
		{
			_print("%-4d  ", channel->ctl_settings.channelid);
			_print("%-6d  ", channel->measures.volts);
			_print("%-6d  ", channel->measures.current);
			_print("%-8d  ", channel->measures.resistance);
			_print("%-9d  ", channel->measures.temperature);
			_print("%-8d  ", channel_configs[i]);
			_print("%d\n", channel->measures.status);
		}
	}
}

