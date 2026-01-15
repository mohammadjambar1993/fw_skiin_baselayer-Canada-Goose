/*
 * heater.c
 *
 *  Created on: Jul. 29, 2021
 *      Author: tmg
 */

#include <string.h>
#include <math.h>
#include "appconfig.h"
#include "heater.h"

#include "tskctrl.h"
#include "soft_pwm.h"
#include "tskctrl.h"
#include "semphr.h"
#include "hal/hal_gpio.h"
#include "util/mya_util.h"
#include "drivers/ina231.h"
#include "drivers/tps65987.h"
#include "temperature_ads.h"

#define ENABLE_LOGGER_HEATER
#ifdef ENABLE_LOGGER_HEATER
	#include "util/logger.h"
	#define logtag	"[hw] "
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
	#define _debug
	#define _info
	#define _warn
	#define _error
#endif

#define MAX_VOLTAGES		10
#define INVALID_VCC			-1
#define DEFAULT_VCC			50
#define ABSOLUTE_VCC_DCH	120

/* Thermistor Coefficients*/

#define TEMP_COEFF		298
#define B_CONSTANT		4096
#define R_CONSTANT		1000
#define TEMP_OFFSET		273.15

//-------- private types --------
typedef struct
{
	bool enabled;
	spwm_channel_t pwmch;
	heat_params_t electric_params;
	const heat_ch_config_t *hw;
}heat_ctrl_ch_t;

typedef struct
{
	int32_t acc; 		//accumulator
	uint16_t samples;	//number of samples
}ina_samples_t;

typedef struct
{
	ina_samples_t voltage;
	ina_samples_t current;
	ina_samples_t short_status;
}ina_measures_t;

//-------- private functions --------
STATIC void tsk_heater(void *params);
STATIC void run_heater_task_loop(void);
STATIC void disable_ads_mux(void);
static bool get_available_voltages(void);
static void print_available_voltages(void);


//-------- private variables --------
//array to hold all channels configurations/states/measurements
static heat_ctrl_ch_t heat_channels[MAX_HEATERS] = {0};
/* temp_channels is used for temporary calculations that will be
 finally loaded into heat_channels. It is used to avoid blocking the
 code with a mutex, in case the heat_channels are accessed directly */
static heat_params_t temp_channels[MAX_HEATERS] = {0};
static ina_measures_t ina_measures[MAX_HEATERS] = {0};
STATIC uint8_t voltages[MAX_VOLTAGES] = {0};
static uint8_t nvoltages = 0; //number of valid voltages
static bool initialized = false;
static SemaphoreHandle_t mtx_chdata = NULL;
static bool change_duty_period = false;
static uint8_t current_vcc = DEFAULT_VCC;
static int16_t new_vcc = INVALID_VCC;
static int16_t pcb_temperature = 0;
static uint32_t new_duty_period = 0;
static bool countmode = false;
static bool request_stop = false;
static bool task_stopped = false;
//-------- private constants --------
/* The control task will run each iteration on CTRL_LOOP_MS.
 * This is also the resolution time base used to drive the PWM signals.
 * Each PWM channel count is incremented at CTRL_LOOP_MS */
STATIC const uint8_t CTRL_LOOP_MS = 20;
STATIC const uint16_t DEFAULT_PWM_PERIOD_MS = (2000/20);

static bool initialize_channels(const heat_ch_config_t *config,
								uint8_t num_channels)
{
	//reset all channels
	memset(heat_channels, 0, sizeof(heat_channels));
	memset(temp_channels, 0, sizeof(temp_channels));
	spwm_channel_t pwmch = SPWM_CH1;
	disable_ads_mux();
	for(uint8_t i = 0; i < num_channels; i++, config++, pwmch++)
	{
		if(config->id >= MAX_HEATERS)
			return false;
		heat_channels[config->id].enabled = true;
		heat_channels[config->id].pwmch = pwmch;
		heat_channels[config->id].hw = config;
		hal_gpio_write(config->heater_output, 0);
		memset(&heat_channels[i].electric_params, 0, sizeof(heat_params_t));
		heat_channels[i].electric_params.channel = config->id;
	}
	return true;
}

STATIC void disable_ads_mux(void)
{
	if(PCB_ID == PCB_MULTI_CHANNEL)
	{
		hal_gpio_clr(TEMP_SW);
		hal_gpio_set(TEMP_E_EN);
	}
	else if(PCB_ID == PCB_DUAL_CHANNEL)
	{
		hal_gpio_clr(TEMP_EN);
	}
}

app_status_t hw_init(const heat_ch_config_t *config, uint8_t num_channels)
{
	//validate parameters and internal state
	if(initialized)
		return APPST_INVALID_STATE;
	if(num_channels > MAX_HEATERS || num_channels == 0)
		return APPST_INVALID_LENGTH;
	if(NULL == config)
		return APPST_INVALID_PARAM;
	//initialize channels and output
	if(!initialize_channels(config, num_channels))
		return APPST_INVALID_PARAM;
	//initialize soft pwm module
	if(!spwm_init(num_channels, DEFAULT_PWM_PERIOD_MS))
		return APPST_INVALID_PARAM;

	request_stop = false;
	task_stopped = false;
	change_duty_period = false;
	new_duty_period = DEFAULT_PWM_PERIOD_MS;
	new_vcc = INVALID_VCC;
	//create mutex to control heating channels parameters
	app_status_t st = os_create_mutex(&mtx_chdata);
	if(st == APPST_OS_ERROR)
	{
		_error("Failed to create mutex");
		return APPST_OS_ERROR;
	}
	//create heater task
	if(NULL == tsk_create(TSK_HEATER_HARDWARE, tsk_heater))
	{
		_error("Failed to create heat ctrl task");
		return APPST_OS_ERROR;
	}
	if(get_available_voltages())
	{
		print_available_voltages();
	}
	else
	{
		_error("Failed to read available voltages");
		return APPST_ERROR;
	}
	memset(ina_measures, 0, sizeof(ina_measures));
	initialized = true;
	return APPST_SUCCESS;
}

void hw_deinit(void)
{
	initialized = false;
	spwm_deinit();
	disable_ads_mux();
}

static bool get_available_voltages(void)
{
	bool success;
	uint8_t nvalues = sizeof(voltages);
	memset(voltages, 0, nvalues);
	success = tps_get_available_voltages(voltages, &nvalues);
	if(success)
	{
		nvoltages = nvalues;
	}
	return success;
}

static void print_available_voltages(void)
{
	uint8_t nvoltages = sizeof(voltages);
	_debug("Available voltages: ");
	for(uint8_t i = 0; i < nvoltages; i++)
	{
		_print("%d   ", voltages[i]);
	}
}

bool hw_is_channel_enabled(heater_id_t ch)
{
	if(!initialized)
		return false;
	if(ch >= MAX_HEATERS)
		return false;
	return heat_channels[ch].enabled;
}

bool hw_is_valid_voltage(uint8_t voltage)
{
#if PCB_ID == PCB_MULTI_CHANNEL
	for(uint8_t i = 0; i < nvoltages; i++)
	{
		if(voltages[i] == voltage)
			return true;
	}
#elif PCB_ID == PCB_DUAL_CHANNEL
	if(ABSOLUTE_VCC_DCH == voltage)
				return true;
#endif
	//print_available_voltages(); //used only for debug
	return false;
}

uint8_t hw_get_highest_voltage(void)
{
	if(!initialized)
		return 0;
	return tps_retrieve_max_volt();
}

uint8_t hw_get_lowest_voltage(void)
{
	if(!initialized)
		return 0;
	return voltages[0];
}

uint8_t hw_get_current_voltage(void)
{
	if(!initialized)
		return 0;
	return current_vcc;
}

app_status_t hw_set_lowest_vcc(void)
{
	if(!initialized || INVALID_VCC != new_vcc)
		return APPST_INVALID_STATE;
	new_vcc = voltages[0];
	return APPST_SUCCESS;
}

app_status_t hw_set_voltage(uint8_t voltage)
{
	if(!initialized || INVALID_VCC != new_vcc)
		return APPST_INVALID_STATE;
	if(!hw_is_valid_voltage(voltage))
	{
		print_available_voltages();
		return APPST_INVALID_PARAM;
	}
	new_vcc = voltage;
	return APPST_SUCCESS;
}

static bool is_valid_channel(heater_id_t ch)
{
	if(ch >= MAX_HEATERS)
		return false;

	return true;
}

uint32_t hw_get_duty_period_ms(void)
{
	if(!initialized)
		return 0;
	return spwm_get_period()*CTRL_LOOP_MS;
}

uint32_t hw_get_duty_period_count(void)
{
	if(!initialized)
		return 0;
	return spwm_get_period();
}

uint32_t hw_get_default_period_count(void)
{
	if(!initialized)
		return 0;
	return DEFAULT_PWM_PERIOD_MS;
}

app_status_t hw_get_pcb_temperature(uint16_t *pcbtemp)
{
	if(!initialized)
		return APPST_INVALID_STATE;
	*pcbtemp = pcb_temperature;
	return APPST_SUCCESS;
}

app_status_t hw_set_duty_period(uint16_t period, bool mode_count)
{
	if(!initialized || change_duty_period)
		return APPST_INVALID_STATE;
	else if(!mode_count && (period < CTRL_LOOP_MS))
		return APPST_INVALID_PARAM;

	new_duty_period = period;
	change_duty_period = true;
	countmode = mode_count;

	return APPST_SUCCESS;
}

app_status_t hw_set_dutycycle(heater_id_t ch, uint8_t duty)
{
	if(!initialized)
		return APPST_INVALID_STATE;
	if(!is_valid_channel(ch) || duty > 100)
		return APPST_INVALID_PARAM;

	heat_ctrl_ch_t *channel = &heat_channels[ch];
	if(spwm_set_duty_cycle(channel->pwmch, duty))
	{
		channel->electric_params.dutycycle = duty;
		return APPST_SUCCESS;
	}
	return APPST_INVALID_PARAM;
}

uint32_t hw_get_ch_ontime_count(heater_id_t ch)
{
	if(!initialized || !is_valid_channel(ch))
		return 0;
	heat_ctrl_ch_t *channel = &heat_channels[ch];
	return spwm_get_ontime(channel->pwmch);
}

app_status_t hw_get_params(heater_id_t ch, heat_params_t *params)
{
	if(!initialized)
		return APPST_INVALID_STATE;
	if(!is_valid_channel(ch) || NULL == params)
		return APPST_INVALID_PARAM;
	heat_ctrl_ch_t *cht = &heat_channels[ch];
	if(os_get_mutex(&mtx_chdata, 5))
	{
		memcpy(params, &cht->electric_params, sizeof(heat_params_t));
		os_release_mutx(&mtx_chdata);
	}
	else
		return APPST_OS_ERROR;
	return APPST_SUCCESS;
}

static void update_power_outputs(void)
{
	heat_ctrl_ch_t *channel = &heat_channels[0];
	for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++)
	{
		if(channel->enabled)
		{
			if(spwm_is_on(channel->pwmch))
				hal_gpio_write(channel->hw->heater_output, 1);
			else
				hal_gpio_write(channel->hw->heater_output, 0);
		}
	}
}

static void read_channels_current(void)
{
	int16_t current;
	cur_status ina_status;
	ina_measures_t *measure = &ina_measures[0];
	heat_ctrl_ch_t *channel = &heat_channels[0];
	for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++, measure++)
	{
		if(!channel->enabled || !spwm_is_on(channel->pwmch))
			continue;
		ina_status = cur_read_current(i, &current); //<<hack>> i should be i2c address
		if(CUR_ST_OK == ina_status)
		{
			measure->current.acc += current;
			measure->current.samples++;
		}
		else
		{
			_warn("Failed to read channel %d current!", i);
		}
		util_blocking_delay_us(500); //give some time to access the bus again
	}
}

static void read_channels_voltage(void)
{
	uint16_t voltage;
	cur_status ina_status;
	ina_measures_t *measure = &ina_measures[0];
	heat_ctrl_ch_t *channel = &heat_channels[0];
	for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++, measure++)
	{
		if(!channel->enabled || !spwm_is_on(channel->pwmch))
			continue;
		ina_status = cur_read_voltage(i, &voltage); //<<hack>> i should be i2c address
		if(CUR_ST_OK == ina_status)
		{
			measure->voltage.acc += voltage;
			measure->voltage.samples++;
		}
		else
		{
			_warn("Failed to read channel %d voltage!", i);
		}
		util_blocking_delay_us(500); //give some time to access the bus again
	}
}

static bool read_channels_status(void)
{
	uint8_t st_channel;
	cur_status ina_status;
	ina_measures_t *measure = &ina_measures[0];
	heat_ctrl_ch_t *channel = &heat_channels[0];
	bool shorted = false;
	for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++, measure++)
	{
		if(!channel->enabled || !spwm_is_on(channel->pwmch))
			continue;
		ina_status = cur_alert_status(i, &st_channel); //<<hack>> i should be i2c address
		shorted |= (st_channel) ? true : false;
		if(CUR_ST_OK == ina_status)
		{
			measure->short_status.acc += st_channel;
			measure->short_status.samples++;
			if(st_channel) //channel is shorted
			{
				hal_gpio_write(channel->hw->heater_output, 0);
			}
		}
		else
		{
			_warn("Failed to read channel %d status!", i);
		}
		util_blocking_delay_us(500); //give some time to access the bus again
	}
	return shorted;
}

static int32_t average_ina_sample(ina_samples_t *sample)
{
	if(sample->samples == 0)
		return 0;
	return (sample->acc)/(sample->samples);
}

static void reset_ina_sample(ina_samples_t *samples)
{
	samples->acc = 0;
	samples->samples = 0;
}

static bool update_measurements(void)
{
	heat_params_t *eparams = &temp_channels[0];
	ina_measures_t *measure = &ina_measures[0];
	heat_ctrl_ch_t *channel = &heat_channels[0];
	if(!os_get_mutex(&mtx_chdata, 5))
	{
		_warn("Cannot acquire heat channel mutex");
		return false;
	}
	for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++, eparams++, measure++)
	{
		if(!channel->enabled)
			continue;
		//get current samples
		channel->electric_params.current = average_ina_sample(&measure->current);
		reset_ina_sample(&measure->current);
		//get voltage samples
		channel->electric_params.volts = average_ina_sample(&measure->voltage);
		reset_ina_sample(&measure->voltage);
		//get short status
		if(measure->short_status.acc > 1)
		{
			channel->electric_params.status = ST_SHORTED;
		}
		else
		{
			channel->electric_params.status = ST_ACTIVE;
		}
		//todo: detect open outputs
		reset_ina_sample(&measure->short_status);
		//get resistance and temperature
		channel->electric_params.resistance = eparams->resistance;
		channel->electric_params.temperature = eparams->temperature;
	}
	os_release_mutx(&mtx_chdata);
	return true;
}

static void power_outputs_off(void)
{
	heat_ctrl_ch_t *channel = &heat_channels[0];
	for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++)
	{
		if(channel->enabled)
		{
			hal_gpio_write(channel->hw->heater_output, 0);
		}
	}
}

static void select_ads_input(heater_id_t id, uint8_t muxaddr)
{
	if(muxaddr > 3)
	{
		_warn("Invalid ADS mux addr: %d", muxaddr);
		return;
	}
	disable_ads_mux();
	if(HEATER_E == id)
	{
		hal_gpio_clr(TEMP_E_EN);//enable channel E
	}
	else
	{
		if(PCB_ID == PCB_MULTI_CHANNEL)
		{
			uint8_t temp_a, temp_b;
			temp_a = (muxaddr & 0b00000010)>>1;
			temp_b = muxaddr & 0b00000001;
			hal_gpio_write(TEMP_A, temp_a);
			hal_gpio_write(TEMP_B, temp_b);
			hal_gpio_set(TEMP_SW);
		}
		else if(PCB_ID == PCB_DUAL_CHANNEL)
		{
			uint8_t temp_sw;
			temp_sw = muxaddr & 0b00000001;
			hal_gpio_write(TEMP_SW, temp_sw);
			hal_gpio_set(TEMP_EN);
		}
	}
}

/* <<hack>> RCONV_COEFF and RCALIBRATION are a temporary hack.
 * These values must be configured during provisioning, stored in
 * flash and passed to this module in the init function */
#define RCONV_COEFF		1
#define RCALIBRATION	1
/*  Disabled for thermistor testing
static int16_t resistance_to_temperature(uint16_t resistance)
{
	float conv_coef = RCONV_COEFF/100.0;
	int16_t temp = conv_coef*(double)resistance-RCALIBRATION;
	return temp;
}
*/

static int16_t resistance_to_temperature_thermistor(uint16_t resistance)
{
	double temp_kelvin;
	int16_t temp_celsius;

	temp_kelvin = (TEMP_COEFF*B_CONSTANT)/(TEMP_COEFF*log((double)(resistance/R_CONSTANT))+B_CONSTANT);
	temp_celsius = (int16_t)(temp_kelvin - TEMP_OFFSET);
	return temp_celsius;
}

static void measure_temperature(void)
{
	disable_ads_mux();
	power_outputs_off();
	os_delay_ms(1);
	heat_ctrl_ch_t *channel = &heat_channels[0];
	heat_params_t *eparams = &temp_channels[0];
	for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++, eparams++)
	{
		if(!channel->enabled)
			continue;
		select_ads_input(i, channel->hw->analog_mux_addr);
		os_delay_ms(1);
		/* <<hack>> the "temperature_measure" function is actually
		 * returning resistance x100. This is going to be fixed during
		 * code refactor */
		eparams->resistance = ads_get_resistance();
		disable_ads_mux();
		eparams->temperature = resistance_to_temperature_thermistor(eparams->resistance);
	}
}

static void check_new_duty_period(void)
{
	uint32_t counter;
	if(change_duty_period)
	{
		if(countmode)
			counter = new_duty_period;
		else
			counter = new_duty_period/CTRL_LOOP_MS;
		if(spwm_set_period(counter))
			_debug("duty period changed to %d - (%d)",new_duty_period,counter);
		else
			_debug("failed to set %d ms duty - (%d)", new_duty_period,counter);
		change_duty_period = false;
	}
}

#define MAX_VCC_TRIES	5
static void check_new_vcc(void)
{
	static uint8_t ct_attempts = 0;
	pmic_status_t status;
	if(INVALID_VCC != new_vcc)
	{
		ct_attempts++;
		status = tps_neg_contract((uint8_t) new_vcc);
		if(PMICST_OK == status)
		{
			current_vcc = (uint8_t)new_vcc;
			_info("Vcc set to %d", new_vcc);
			new_vcc = INVALID_VCC;
			ct_attempts = 0;
			os_delay_ms(1); //give some time for the new vcc to settle
		}
		else if(ct_attempts <= MAX_VCC_TRIES)
		{
			_warn("Failed setting VCC: %d. (try %d)", status, ct_attempts);
		}
		else
		{
			memset(voltages, 0, sizeof(voltages));
			_info("Cannot set %d vcc", new_vcc);
			new_vcc = INVALID_VCC;
			ct_attempts = 0;
		}
	}
}

STATIC void run_heater_task_loop(void)
{
	bool shorted_output;
	static TickType_t delaytime = 0;
	check_new_vcc();
	check_new_duty_period();
	update_power_outputs();
	os_delay_ms(1);
	read_channels_current();
	read_channels_voltage();
	shorted_output = read_channels_status();
	if(shorted_output)
		os_evt_trigger(EVT_HEAT_SHORT);
	if(spwm_increment()) //true when a new PWM period starts
	{
		measure_temperature();
		pcb_temperature = ads_get_PCB_temperature();
		if(update_measurements() && !shorted_output)
		{
			os_evt_trigger(EVT_HEAT_SAMPLE);
		}
	}
	os_delay_until_ms(20, &delaytime);
}

STATIC void init_ina231(void)
{
	heat_ctrl_ch_t *channel = &heat_channels[0];
	for(uint8_t i = 0; i < MAX_HEATERS; i++, channel++)
	{
		if(channel->enabled)
		{
			/* <<hack>> 1) these parameters were copied from older version and
			 * need to be checked! 2) cur_sensor_init should receive the
			 * i2c address and not define it inside the implementation */
			if(!cur_sensor_init(i, 20, 1500))
				_error("Failed to initialize INA %d", i);
			os_delay_ms(2); //wait some time to access the bus again
		}
	}
}


/* this function is experimental and was implemented to stop the control logic
 * and heater module when writing new configuration into ADS. It is assumed that
 * after the ADS is re-configured, the pod will reset itself, so no "resume"
 * function is provided */
void hw_stop(void)
{
	request_stop = true;
}

bool hw_is_task_stopped(void)
{
	return task_stopped;
}

STATIC void tsk_heater(void *params)
{
	init_ina231();
	if(!ads_init())
	{
		_error("Failed to initialize ADS");
	}
	os_delay_ms(10);
	new_vcc = DEFAULT_VCC; //force vcc update when tasks starts
	while(1)
	{
		if(task_stopped)
		{
			os_delay_ms(1000);
		}
		else
			run_heater_task_loop();
		if(request_stop)
		{
			power_outputs_off();
			task_stopped = true;
			request_stop = false;
		}
	}
}
