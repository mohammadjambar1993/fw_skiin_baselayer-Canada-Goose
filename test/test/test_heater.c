#ifdef TEST

#include "unity.h"
#include "soft_pwm.h"
#include "hal/hal_gpio.h"
#include "hal/hal_i2c.h"
#include "util/mya_util.h"
#include "mock_tskctrl.h"
#include "mock_hal_gpio.h"
#include "mock_logger.h"
#include "mock_ina231.h"
#include "mock_tps65987.h"
#include "mock_temperature_ads.h"
#include "mock_SEGGER_RTT.h"
#include "heater.h"

#define FAKE_TASK_HANDLE ((uint32_t*)1)

//static declarations from heater module
extern void tsk_heater(void *params);
extern void run_heater_task_loop(void);
extern const uint16_t DEFAULT_PWM_PERIOD_MS;
extern const uint8_t CTRL_LOOP_MS;
extern uint8_t voltages[10];

static heat_ch_config_t cfg_all_channels[] =
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
		.analog_mux_addr = 1
	},
	{//channel C
		.id = HEATER_C,
		.heater_output = HEATING_CHC,
		.addr_ina231 = 0x44,
		.analog_mux_addr = 2
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
		.analog_mux_addr = 0
	},
};

static uint8_t nchannels;

void setUp(void)
{
	nchannels = sizeof(cfg_all_channels)/sizeof(cfg_all_channels[0]);
	log_debug_Ignore();
	log_error_Ignore();
	log_info_Ignore();
	os_delay_ms_Ignore();
	os_delay_until_ms_Ignore();
	cur_read_current_IgnoreAndReturn(CUR_ST_OK);
	cur_read_voltage_IgnoreAndReturn(CUR_ST_OK);
	cur_alert_status_IgnoreAndReturn(CUR_ST_OK);
	hal_gpio_clr_Ignore();
	hal_gpio_set_Ignore();
	tps_get_available_voltages_IgnoreAndReturn(true);
	SEGGER_RTT_SetTerminal_IgnoreAndReturn(0);
	SEGGER_RTT_printf_IgnoreAndReturn(0);
	temperature_measure_IgnoreAndReturn(0);
}

void tearDown(void)
{
	hw_deinit();
}

void test_heater_task_creation(void)
{
	uint8_t voltages[10], nvoltages;
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	//check module initialization error
	tsk_create_ExpectAndReturn(TSK_HEATER_HARDWARE, tsk_heater, NULL);
    TEST_ASSERT_EQUAL_UINT(APPST_OS_ERROR, hw_init(cfg_all_channels, nchannels));
    hw_deinit();
    //check module initialization success
    tsk_create_ExpectAndReturn(TSK_HEATER_HARDWARE, tsk_heater, FAKE_TASK_HANDLE);
    TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
}

#define EXPECT_TASK_CREATION() tsk_create_ExpectAndReturn(TSK_HEATER_HARDWARE, tsk_heater, FAKE_TASK_HANDLE)
void test_heater_initialization(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	//initialize on success
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	hw_deinit();
	//test re-initialization without deinit
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_STATE, hw_init(cfg_all_channels, nchannels));
	hw_deinit();
	//initialize error due to wrong number of channels
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_LENGTH, hw_init(cfg_all_channels, nchannels+1));
	hw_deinit();
	//initialize error due to zero channels
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_LENGTH, hw_init(cfg_all_channels, 0));
	hw_deinit();
	//initialize error due to NULL parameter
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, hw_init(NULL, nchannels));
}

void test_heater_heater_outputs_disabled_on_init(void)
{
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	for(uint8_t i = 0; i < nchannels; i++)
	{
		hal_gpio_write_Expect(cfg_all_channels[i].heater_output, 0);
	}
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
}

void test_heater_channel_init_invalid_id(void)
{
	hal_gpio_write_Ignore();
	tsk_create_IgnoreAndReturn(FAKE_TASK_HANDLE);
	cfg_all_channels[nchannels-1].id = MAX_HEATERS;
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, hw_init(cfg_all_channels, nchannels));
	cfg_all_channels[nchannels-1].id = HEATER_E;
}

void test_heater_set_duty(void)
{
	uint8_t duty;
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	tsk_create_IgnoreAndReturn(FAKE_TASK_HANDLE);
	//set when module is not initialized
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_STATE, hw_set_dutycycle(HEATER_A, 50));
	//set invalid channel when all channels are enabled
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, hw_set_dutycycle(HEATER_E+1, 50));
	hw_deinit();
	//set invalid channel when not all channels are enabled
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels-1));
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, hw_set_dutycycle(HEATER_E, 50));
	//set invalid duty
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, hw_set_dutycycle(HEATER_D, 101));
	//set valid channels/duty
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_dutycycle(HEATER_D, 0));
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_dutycycle(HEATER_B, 1));
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_dutycycle(HEATER_A, 50));
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_dutycycle(HEATER_C, 99));
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_dutycycle(HEATER_C, 100));
}

typedef struct
{
	heater_id_t id;
	uint8_t duty_cycle;
	spwm_channel_t pwmch;
	uint16_t timeon;
	uint16_t timeoff;
}pwm_test_t;

void test_heater_channels_duty_in_control_loop(void)
{
	pwm_test_t *ch;
	pwm_test_t channels[] =
	{
		{HEATER_A, 0, 	SPWM_CH1, 0, 0},
		{HEATER_B, 1, 	SPWM_CH2, 0, 0},
		{HEATER_C, 59, 	SPWM_CH3, 0, 0},
		{HEATER_D, 99, 	SPWM_CH4, 0, 0},
		{HEATER_E, 100, SPWM_CH5, 0, 0},
	};
	uint8_t num_channels = sizeof(channels)/sizeof(channels[0]);
	hal_gpio_write_Ignore();
	os_delay_ms_Ignore();
	os_evt_trigger_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	//create task
	tsk_create_ExpectAndReturn(TSK_HEATER_HARDWARE, tsk_heater, FAKE_TASK_HANDLE);
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	//set all duty cycles
	ch = channels;
	for(uint8_t i = 0; i < num_channels; i++, ch++)
	{
		TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_dutycycle(ch->id, ch->duty_cycle));
	}
	//run control logic for a full cycle and measure the duty cycle
	uint16_t timetotal = 0;
	for(uint16_t i = 0; i < DEFAULT_PWM_PERIOD_MS; i++)
	{
		run_heater_task_loop();
		timetotal++;
		ch = channels;
		//check each pwm channel state (high or low)
		for(uint8_t k = 0; k < num_channels; k++, ch++)
		{
			if(spwm_is_on(ch->pwmch))
				ch->timeon++;
			else
				ch->timeoff++;
		}
	}
	//validate the duty cycle
	ch = channels;
	float measured_duty;
	for(uint8_t i = 0; i < num_channels; i++, ch++)
	{
		TEST_ASSERT_EQUAL_UINT(ch->timeon+ch->timeoff, timetotal);
		measured_duty = ((float)ch->timeon*100)/timetotal;
		TEST_ASSERT_FLOAT_WITHIN(0.1, ch->duty_cycle, measured_duty);
	}
}

/* at the end of each PWM cycle, the heat module must generate a
 * EVT_HEAT_SAMPLE event, which indicates  */
void test_heater_measurement_event_in_control_loop(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	os_get_mutex_IgnoreAndReturn(true);
	os_release_mutx_Ignore();
	temperature_sens_meas_Ignore();
	temperature_board_get_IgnoreAndReturn(2334);
	//check if the event is triggered
	tsk_create_ExpectAndReturn(TSK_HEATER_HARDWARE, tsk_heater, FAKE_TASK_HANDLE);
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	os_evt_trigger_Expect(EVT_HEAT_SAMPLE);
	for(uint16_t i = 0; i < DEFAULT_PWM_PERIOD_MS+1; i++)
	{
		run_heater_task_loop();
	}
	//read channels data
}

void test_heater_get_params_validate_args(void)
{
	heat_params_t params;
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	os_get_mutex_IgnoreAndReturn(true);
	os_release_mutx_Ignore();
	hal_gpio_write_Ignore();
	//reading without initializing the module
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_STATE, hw_get_params(HEATER_C, &params));
	//initialize module before other tests
	tsk_create_ExpectAndReturn(TSK_HEATER_HARDWARE, tsk_heater, FAKE_TASK_HANDLE);
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	//invalid channel
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, hw_get_params(HEATER_E+1, &params));
	//invalid parameters
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, hw_get_params(HEATER_A, NULL));
	//test with valid inputs
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_get_params(HEATER_B, &params));
}

void test_heater_get_channel_enabled(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	//initialize on success
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels-1));
	//channels A,B,C and D should be enabled. E should be disabled
	TEST_ASSERT_TRUE(hw_is_channel_enabled(HEATER_A));
	TEST_ASSERT_TRUE(hw_is_channel_enabled(HEATER_B));
	TEST_ASSERT_TRUE(hw_is_channel_enabled(HEATER_C));
	TEST_ASSERT_TRUE(hw_is_channel_enabled(HEATER_D));
	TEST_ASSERT_FALSE(hw_is_channel_enabled(HEATER_E));
}

void test_heater_set_period_milliseconds_mode(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	//initialize on success
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	//fail if period is less than the minimum
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, hw_set_duty_period(CTRL_LOOP_MS-1, false));
	//test success
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_duty_period(3000, false));
}

void test_heater_set_period_counter_mode(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	//initialize on success
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	//test success
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_duty_period(1000, true));
}

void test_heater_get_duty_period_ms(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	TEST_ASSERT_EQUAL_UINT(2000, hw_get_duty_period_ms());
}

void test_heater_get_duty_period_count(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	TEST_ASSERT_EQUAL_UINT(100, hw_get_duty_period_count());
}

void test_heater_get_channel_on_count(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_duty_period(3000, false));
	run_heater_task_loop();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_dutycycle(HEATER_C, 50));
	TEST_ASSERT_EQUAL_UINT(75, hw_get_ch_ontime_count(HEATER_C));
}

void test_heater_set_voltages(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	//init voltages array
	uint8_t nvoltages = sizeof(voltages);
	for(uint8_t i = 0; i < nvoltages; i++)
	{
		voltages[i] = i;
	}
	//test all valid voltages
	for(uint8_t i = 0; i < nvoltages; i++)
	{
		tps_neg_contract_ExpectAndReturn(voltages[i], PMICST_OK);
		TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_voltage(voltages[i]));
		run_heater_task_loop();
	}
	//test an invalid voltage
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, hw_set_voltage(nvoltages+1));
}

void test_heater_set_lowest_voltage(void)
{
	hal_gpio_write_Ignore();
	os_create_mutex_IgnoreAndReturn(APPST_SUCCESS);
	EXPECT_TASK_CREATION();
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_init(cfg_all_channels, nchannels));
	//init voltages array
	uint8_t nvoltages = sizeof(voltages);
	for(uint8_t i = 0; i < nvoltages; i++)
	{
		voltages[i] = i+10;
	}
	tps_neg_contract_ExpectAndReturn(voltages[0], APPST_SUCCESS);
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, hw_set_lowest_vcc());
	run_heater_task_loop();
}

#endif // TEST
