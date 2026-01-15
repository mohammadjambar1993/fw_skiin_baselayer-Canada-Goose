#include "hal_config.h"
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833)

#include "hal_pwm.h"
#include "nrf_drv_pwm.h"
#include "logger.h"
#include "diagnostic.h"
//#include "nrfx_pwm.h"

#define MIN_FREQ_HZ    31       //minimum frequency when clock is 1MHz
#define MAX_FREQ_HZ    8000000  //maximum frequency when clock is 1MHz

#if DEVKIT_NRF52 == 1 //running code using PCA10040 development kit

#define HEATER_CONTROL_A    11
#define HEATER_CONTROL_B    12
#define HEATER_CONTROL_C    13
#define HEATER_CONTROL_D    14

#else
#define HEATER_CONTROL_A    12
#define HEATER_CONTROL_B    8
#define HEATER_CONTROL_C    13
#define HEATER_CONTROL_D    11
#endif //DEVKIT_NRF52
#define N_PWMS        4

static PWMStatus status = PWMST_ERROR;

static nrf_drv_pwm_t pwm = NRF_DRV_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t seq_values[] = {0};
static nrf_pwm_sequence_t const seq =
{
    .values.p_individual = seq_values,
    .length          = NRF_PWM_VALUES_LENGTH(seq_values),
    .repeats         = 0,
    .end_delay       = 0
};

static nrf_drv_pwm_config_t config =
{
    .output_pins =
    {
    	HEATER_CONTROL_A,
		HEATER_CONTROL_B,
		HEATER_CONTROL_C,
		HEATER_CONTROL_D,
    },
    .irq_priority = APP_IRQ_PRIORITY_LOWEST,
    .base_clock   = NRF_PWM_CLK_8MHz,
    .count_mode   = NRF_PWM_MODE_UP,
    /* Dont forget to change MIN_FREQ_HZ and MAX_FREQ_HZ when when top_value is
     * changed!*/
    .top_value    = 160, //8 MHz clock, 160 divider = 50KHz
    .load_mode    = NRF_PWM_LOAD_INDIVIDUAL,
    .step_mode    = NRF_PWM_STEP_AUTO
};

PWMStatus hal_pwm_init(uint32_t freq_hz)
{
    if(freq_hz < MIN_FREQ_HZ || freq_hz > MAX_FREQ_HZ)
    {
        log_error("Wrong pwm frequency: %d", freq_hz);
        return PWMST_INV_FREQ;
    }
    config.top_value = (uint16_t)(MAX_FREQ_HZ/freq_hz);
    status = PWMST_OK;
    nrf_drv_pwm_uninit(&pwm);
    uint32_t ret = nrf_drv_pwm_init(&pwm, &config, NULL);
    if(NRF_SUCCESS != ret)
    {
        log_error("pwm initialization : %d", ret);
        diag_inc_flag(FLG_PWM_INIT );
        status = PWMST_ERROR;
    }
    return status;
}

PWMStatus hal_pwm_set_duty(PWMCh channel, uint16_t duty)
{
    if(PWMST_OK != status)
        return status;
    if(duty > 100)
        return PWMST_INV_DUTY;
    if(channel >= MAX_PWMS || channel >= N_PWMS)
        return PWMST_INV_CH;

    float fduty = duty/100.0;
    fduty = (config.top_value*fduty)+0.5;
    duty = config.top_value-((uint16_t)fduty);
    if(0 == channel)
        seq_values->channel_0 = duty;
    else if(1 == channel)
        seq_values->channel_1 = duty;
    else if(2 == channel)
        seq_values->channel_2 = duty;
    else if(3 == channel)
        seq_values->channel_3 = duty;
    else
        return PWMST_INV_CH;

    nrf_drv_pwm_simple_playback(&pwm, &seq, 1, NRF_DRV_PWM_FLAG_LOOP);
    return PWMST_OK;
}

#endif //UC_ID == UC_NRF52832

