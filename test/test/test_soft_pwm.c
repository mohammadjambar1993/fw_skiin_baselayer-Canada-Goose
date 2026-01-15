#ifdef TEST

#include <math.h>
#include "unity.h"
#include "soft_pwm.h"

static uint32_t period;

void setUp(void)
{
	period = 100;
}

void tearDown(void)
{
	spwm_deinit();
}

static bool set_all_channels_duty(uint8_t maxduty)
{
	uint8_t duty;
	spwm_channel_t i;
	for(i = SPWM_CH1; i < MAX_SPWM_CH; i++)
	{
		for(duty = 0; duty <= maxduty; duty++)
		{
			if(!spwm_set_duty_cycle(i, duty))
			{
				//TEST_PRINTF("Failed on channel %d, duty: %d", i, duty);
				return false;
			}
		}
	}
	return true;
}

void test_spwm_success_on_initialization(void)
{
	period = 100;
	//initialize all possible # of channels
	for(uint8_t i = 1; i <= MAX_SPWM_CH; i++)
	{
		TEST_ASSERT_TRUE(spwm_init(i, period));
		spwm_deinit();
	}
}

void test_spwm_fail_on_initialization(void)
{
	//fail on multiple initializations
	TEST_ASSERT_TRUE(spwm_init(1, period)); //1st initialization must work
	TEST_ASSERT_FALSE(spwm_init(1, period)); //2nd must fail
	spwm_deinit();
	//fail to initialize beyond maximum channels
	TEST_ASSERT_FALSE(spwm_init(MAX_SPWM_CH+1, period));
	spwm_deinit();
	//fail to initialize no channel
	TEST_ASSERT_FALSE(spwm_init(0, period));
	spwm_deinit();
	//period cannot be zero
	period = 0;
	TEST_ASSERT_FALSE(spwm_init(1, period));
	spwm_deinit();
}

void test_spwm_set_duty(void)
{
	//before module initialization
	TEST_ASSERT_FALSE(spwm_set_duty_cycle(SPWM_CH1, 50));
	//invalid channel
	TEST_ASSERT_TRUE(spwm_init(MAX_SPWM_CH, period));
	TEST_ASSERT_FALSE(spwm_set_duty_cycle(MAX_SPWM_CH, 50));
	//invalid duty value
	TEST_ASSERT_FALSE(set_all_channels_duty(101));
	//valid duty values
	TEST_ASSERT_TRUE(set_all_channels_duty(100));
}

void test_spwm_increment_when_not_initialized(void)
{
	TEST_ASSERT_FALSE(spwm_increment());
}

void test_spwm_get_period(void)
{
	uint32_t period = 237;
	TEST_ASSERT_TRUE(spwm_init(5, period));
	TEST_ASSERT_EQUAL_UINT(period, spwm_get_period());
}

void test_spwm_update_and_overflow(void)
{
	period = 333;
	spwm_deinit();
	TEST_ASSERT_TRUE(spwm_init(MAX_SPWM_CH, period));
	for(uint32_t i = 0; i < period; i++)
	{
		TEST_ASSERT_FALSE(spwm_increment());
	}
	TEST_ASSERT_TRUE(spwm_increment());
}

static float measure_duty_cycle(spwm_channel_t ch, uint8_t numcycles)
{
	uint32_t iterations_total = 0;
	uint32_t iterations_on = 0;
	uint32_t iterations_off = 0;

	for(uint8_t i = 0; i < numcycles; i++)
	{
		while(!spwm_increment())
		{
			iterations_total++;
			if(spwm_is_on(ch))
				iterations_on++;
			else
				iterations_off++;
		}
	}
	TEST_ASSERT_EQUAL_UINT(iterations_on+iterations_off, iterations_total);
	float expected_duty = ((float)iterations_on*100)/iterations_total;
	return expected_duty;
}

void test_spwm_measure_duty_all_channels(void)
{
	period = 200;
	uint8_t numcycles = 3;
	spwm_channel_t channel = SPWM_CH1;
	float expected_duty, measured_duty;

	spwm_deinit();
	TEST_ASSERT_TRUE(spwm_init(MAX_SPWM_CH, period));

	//measure all possible PWM values with different number of cycles
	for(channel = SPWM_CH1; channel < MAX_SPWM_CH; channel++)
	{
		for(uint8_t cycles = 1; cycles <= numcycles; cycles++)
		{
			for(uint8_t duty = 0; duty <= 100; duty++)
			{
				TEST_ASSERT_TRUE(spwm_set_duty_cycle(channel, duty));
				expected_duty = (float)duty;
				measured_duty = measure_duty_cycle(channel, cycles);
				//TEST_PRINTF("%f, %f", expected_duty, measured_duty);
				TEST_ASSERT_FLOAT_WITHIN(0.9999, expected_duty, measured_duty);
			}
		}
	}
}

void test_spwm_get_duty_cycle(void)
{
	uint8_t duty;
	//get duty when not initialized
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_STATE, spwm_get_duty_cycle(SPWM_CH1, &duty));
	//get pwm when duty pointer is invalid
	TEST_ASSERT_TRUE(spwm_init(MAX_SPWM_CH, 100));
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, spwm_get_duty_cycle(SPWM_CH2, NULL));
	//get duty from invalid channel
	TEST_ASSERT_EQUAL_UINT(APPST_INVALID_PARAM, spwm_get_duty_cycle(MAX_SPWM_CH, &duty));
	//set and get duty
	duty = 0;
	TEST_ASSERT_TRUE(spwm_set_duty_cycle(SPWM_CH3, 75));
	TEST_ASSERT_EQUAL_UINT(APPST_SUCCESS, spwm_get_duty_cycle(SPWM_CH3, &duty));
	TEST_ASSERT_EQUAL_UINT(75, duty);
}

void test_spwm_set_period(void)
{
	float measured_duty;
	uint8_t expected_duty = 77;
	uint32_t period = 1000, newperiod = 150;
	TEST_ASSERT_TRUE(spwm_init(MAX_SPWM_CH, period));
	TEST_ASSERT_TRUE(spwm_set_duty_cycle(SPWM_CH3, expected_duty));
	TEST_ASSERT_TRUE(spwm_set_period(newperiod));
	measured_duty = measure_duty_cycle(SPWM_CH3, 3);
	TEST_ASSERT_FLOAT_WITHIN(0.9999, expected_duty, measured_duty);
}

void test_spwm_get_channel_on_time(void)
{
	float measured_duty;
	uint8_t expected_duty = 77;
	uint32_t period = 1000;
	TEST_ASSERT_TRUE(spwm_init(MAX_SPWM_CH, period));
	TEST_ASSERT_TRUE(spwm_set_duty_cycle(SPWM_CH3, expected_duty));
	TEST_ASSERT_EQUAL_UINT(spwm_get_ontime(SPWM_CH3), 770);
}

#endif // TEST
