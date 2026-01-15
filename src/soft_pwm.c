#include <math.h>
#include <string.h>
#include "soft_pwm.h"

typedef struct
{
	bool enabled;
	uint8_t duty_cycle;
	uint32_t ct;
	uint32_t period;
}channel_t;

static bool initialized = false;
static uint32_t max_period = 0;
static uint32_t ct_period = 0;
static channel_t channels[MAX_SPWM_CH] = {0};

static void reset_channels(void);
static void increment_channels(void);

bool spwm_init(uint8_t nchannels, uint32_t period)
{
	if(initialized)
		return false;
	if(nchannels > MAX_SPWM_CH || nchannels == 0)
		return false;
	if(period == 0)
		return false;

	memset(channels, 0, sizeof(channels));
	reset_channels();
	max_period = period;
	ct_period = 0;
	initialized = true;
	return initialized;
}

void spwm_deinit(void)
{
	initialized = false;
}

uint32_t spwm_get_period(void)
{
	return max_period;
}

bool spwm_set_period(uint32_t period)
{
	if(!initialized)
		return false;
	if(max_period == period)
		return true;
	max_period = period;
	ct_period = 0;
	for(spwm_channel_t ch = SPWM_CH1; ch < MAX_SPWM_CH; ch++)
	{
		spwm_set_duty_cycle(ch, channels[ch].duty_cycle);
	}
	return true;
}

uint32_t spwm_get_ontime(spwm_channel_t ch)
{
	if(!initialized || ch >= MAX_SPWM_CH)
		return 0;
	channel_t *channel = &channels[ch];
	return channel->period;
}

bool spwm_set_duty_cycle(spwm_channel_t ch, uint8_t duty)
{
	if(!initialized || ch >= MAX_SPWM_CH)
		return false;
	if(duty > 100)
		return false;

	channel_t *channel = &channels[ch];
	float fduty = (duty/100.0);
	channel->period = (uint32_t)roundf(fduty*max_period);
	if(channel->period > max_period)
		channel->period = max_period;
	channel->enabled = (channel->period > 0);
	channel->duty_cycle = duty;
	return true;
}

app_status_t spwm_get_duty_cycle(spwm_channel_t ch, uint8_t *duty)
{
	if(!initialized)
		return APPST_INVALID_STATE;
	if(NULL == duty || ch >= MAX_SPWM_CH)
		return APPST_INVALID_PARAM;
	*duty = channels[ch].duty_cycle;
	return APPST_SUCCESS;
}

bool spwm_increment(void)
{
	if(!initialized)
		return false;
	if(max_period == 0)
		return false;
	ct_period++;
	increment_channels();
	if(ct_period >= max_period+1)
	{
		ct_period = 0; //start next cycle
		reset_channels();
	}
	return (ct_period == 0); //returns true when a new cycle starts
}

bool spwm_is_on(spwm_channel_t ch)
{
	if(!initialized)
		return false;
	if(ch >= MAX_SPWM_CH)
		return false;
	channel_t *channel = &channels[ch];
	if(!channel->enabled || max_period == 0)
		return false;
	return (channel->ct <= channel->period);
}

static void reset_channels(void)
{
	spwm_channel_t ch;
	channel_t *channel = &channels[SPWM_CH1];
	for(ch = SPWM_CH1; ch < MAX_SPWM_CH; ch++, channel++)
	{
		channel->ct = 0;
	}
}

static void increment_channels(void)
{
	spwm_channel_t ch;
	channel_t *channel = &channels[SPWM_CH1];
	for(ch = SPWM_CH1; ch < MAX_SPWM_CH; ch++, channel++)
	{
		if(channel->enabled && (channel->ct <= channel->period))
		{
			channel->ct++;
		}
	}
}
