#ifndef SOURCE_HAL_PWM_H_
#define SOURCE_HAL_PWM_H_

#include "hal_config.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    PWMST_OK        = 0,
    PWMST_ERROR     = 1,
    PWMST_INV_FREQ  = 2, //invalid frequency
    PWMST_INV_STATE = 3, //invalid internal state
    PWMST_INV_DUTY  = 4, //invalid duty cycle
    PWMST_INV_CH    = 5, //invalid PWM channel
}PWMStatus;

typedef enum
{
    //outputs
	PWMCH_0 = 0,
	PWMCH_1 = 1,
	PWMCH_2 = 2,
#define PWMS_LEGGING PWMCH_2
	PWMCH_3 = 3,
	PWMCH_4 = 4,
    //dont define pins below MAX_PWMS
    MAX_PWMS,
}PWMCh;

PWMStatus hal_pwm_init(uint32_t freq_hz);
PWMStatus hal_pwm_set_duty(PWMCh channel, uint16_t duty);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SOURCE_HAL_PWM_H_ */
