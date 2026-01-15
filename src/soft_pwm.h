#ifndef SOFT_PWM_H
#define SOFT_PWM_H

#include <stdbool.h>
#include <stdint.h>
#include "apptypes.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
	SPWM_CH1 = 0,
	SPWM_CH2 = 1,
	SPWM_CH3 = 2,
	SPWM_CH4 = 3,
	SPWM_CH5 = 4,
	//dont define new channels beyond SPWM_CH_MAX
	MAX_SPWM_CH
}spwm_channel_t;

bool spwm_init(uint8_t nchannels, uint32_t period);
void spwm_deinit(void);
uint32_t spwm_get_period(void);
bool spwm_set_period(uint32_t period);
uint32_t spwm_get_ontime(spwm_channel_t ch);
bool spwm_set_duty_cycle(spwm_channel_t ch, uint8_t duty);
bool spwm_increment(void);
bool spwm_is_on(spwm_channel_t ch);
app_status_t spwm_get_duty_cycle(spwm_channel_t ch, uint8_t *duty);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif // SOFT_PWM_H
