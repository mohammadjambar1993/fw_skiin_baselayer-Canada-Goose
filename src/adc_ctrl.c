/*
 * adc_ctrl.c
 *
 *  Created on: Oct 11, 2017
 *      Author: Myant
 */

#include <string.h>
#include "adc_ctrl.h"
#include "appconfig.h"
#include "logger.h"
#include "nrf_drv_ppi.h"
#include "nrf_drv_timer.h"
#include "nrfx_saadc.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_rtc.h"
#include "tskctrl.h"
#include "hal_gpio.h"
#include "mya_util.h"
#include "diagnostic.h"

#define MODE_INT    2
#define MODE_PPI    1
#define MODE_RTC    0
#define ADC_MODE    3  //other mode,not MODE_PPI,nor RTC
///when RTC mode is enabled, RTC_ID and RTC_CC_TIME are used to control  RTC
#define RTC_ID      2   //which RTC is used
#define RTC_CHANNEL 0   //rtc capture channel
//frequency that RTC will trigger an ADC conversion: (32768 Hz / RTC_CC_TIME)
#define RTC_CC_TIME     1639 //3277: ~10Hz, 328: ~100Hz

#define PPI_SG_10HZ     1600000
#define PPI_SG_100HZ    160000

#if ADC_MODE == MODE_PPI
#define PPI_SG_SAMPLING PPI_SG_10HZ
#endif

#define PRINT_CONVERSION    0 // when 1 print results using the logger
//number of samples for each input
#define NSAMP               1

#define ADC_SAMPLES (NSAMP*MAX_ADC_CH)
static nrf_saadc_value_t adc_buffer[ADC_SAMPLES];
static nrf_saadc_value_t conversion[ADC_SAMPLES] = {0};
static bool configured = false;

static bool mode_enabled = false;
#if ADC_MODE == MODE_PPI
static nrf_ppi_channel_t ppich, ppisync;
static void timer_event_handler(nrf_timer_event_t event_type, void* p_context)
{}
#endif
#if ADC_MODE == MODE_RTC
static nrf_drv_rtc_t rtc = NRF_DRV_RTC_INSTANCE(RTC_ID);
static void irq_rtc(nrf_drv_rtc_int_type_t int_type);
#endif

static const nrf_saadc_channel_config_t channels[] =
{       
    {//AIN7 - V_HEATER_PSU_SENSE
        .reference = NRF_SAADC_REFERENCE_VDD4,
        .gain = NRF_SAADC_GAIN1_4,
        .acq_time = NRF_SAADC_ACQTIME_20US,
        .mode = NRF_SAADC_MODE_SINGLE_ENDED,
        .pin_p = NRF_SAADC_INPUT_AIN1, //P0.3
        .pin_n = NRF_SAADC_INPUT_DISABLED,
        .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED
    },
};

static const uint8_t NCHANNELS = sizeof(channels)/sizeof(channels[0]);
static volatile bool adc_finished = false;

static void config_adc(void);
static void config_ppi_mode(void)__attribute__((unused));
static void config_rtc_mode(void)__attribute__((unused));
static void saadc_callback(nrf_drv_saadc_evt_t const *p_event);

void adc_init(void)
{
    if(configured)
        return;
    mode_enabled = false;
    if(NCHANNELS != MAX_ADC_CH)
    {
        log_error("[adc] bad channels declaration");
        return;
    }
    nrf_drv_saadc_uninit();
    config_adc();
#if ADC_MODE == MODE_PPI
    config_ppi_mode();
#elif ADC_MODE == MODE_RTC
    config_rtc_mode();
#endif
    configured = true;
}

static void config_adc(void)
{
    uint8_t i;
    nrf_drv_saadc_config_t sadc;
    sadc.resolution = NRF_SAADC_RESOLUTION_12BIT;
    sadc.oversample = NRF_SAADC_OVERSAMPLE_DISABLED;
    sadc.interrupt_priority = APP_IRQ_PRIORITY_LOW;
    nrf_drv_saadc_init(&sadc, saadc_callback);
    //initialize channels
    for(i = 0; i < NCHANNELS; i++)
        nrf_drv_saadc_channel_init(i, &channels[i]);
}

uint8_t adc_get_mode(void)
{
    return ADC_MODE;
}

static void config_ppi_mode(void)
{
#if ADC_MODE == MODE_PPI
    //config PPI
    nrf_drv_timer_t timer = NRF_DRV_TIMER_INSTANCE(TIMER_ADC);
    nrf_drv_timer_init(&timer, NULL, timer_event_handler);
    nrf_drv_timer_extended_compare(&timer, NRF_TIMER_CC_CHANNEL1, PPI_SG_SAMPLING,
            NRF_TIMER_SHORT_COMPARE1_CLEAR_MASK, false);
    uint32_t evttimer = nrf_drv_timer_compare_event_address_get(&timer,
            NRF_TIMER_CC_CHANNEL1);
    nrf_drv_timer_enable(&timer);

    nrf_drv_ppi_channel_alloc(&ppich);
    nrf_drv_ppi_channel_assign(ppich,evttimer,nrf_drv_saadc_sample_task_get());

    nrf_drv_ppi_channel_alloc(&ppisync);
    nrf_drv_ppi_channel_assign(ppisync,
            (uint32_t)nrf_saadc_event_address_get(NRF_SAADC_EVENT_END),
            nrf_saadc_task_address_get(NRF_SAADC_TASK_START));
#endif
}

static void config_rtc_mode(void)
{
#if ADC_MODE == MODE_RTC
	nrf_drv_rtc_config_t p_config;
	p_config.prescaler= 0;
	p_config.interrupt_priority=APP_IRQ_PRIORITY_HIGH;
	p_config.reliable=0;
	p_config.tick_latency=((2000*32768)/1000000);
    nrf_drv_rtc_init(&rtc, &p_config, irq_rtc);
    nrf_drv_rtc_cc_set(&rtc, RTC_CHANNEL, RTC_CC_TIME, true);
    nrf_drv_rtc_enable(&rtc);
#endif
}

void adc_uninit(void)
{
    if(configured)
    {
    #if ADC_MODE == MODE_PPI
        nrf_drv_ppi_channel_disable(ppich);
        nrf_drv_ppi_channel_disable(ppisync);
    #elif ADC_MODE == MODE_RTC
        nrf_drv_rtc_counter_clear(&rtc);
        nrf_drv_rtc_uninit(&rtc);
    #endif
        nrf_drv_saadc_abort();
        nrf_drv_saadc_uninit();
        configured = false;
        mode_enabled = false;
}
}

bool adc_sample(void)
{
	bool adc_ok;
	adc_finished = adc_ok = false;
	config_adc();
    nrf_drv_saadc_buffer_convert(adc_buffer, ADC_SAMPLES);
    nrf_drv_saadc_sample();
    adc_ok = util_check_flag_us(&adc_finished, 2000);//block for 2ms at maximum
    if(adc_finished||adc_ok)
    	return true;
    diag_inc_flag(FLG_ADC_IRQ);
    return false;

}

uint16_t adc_read(adc_channel_e ch)
{
    if(!configured || (ch >= MAX_ADC_CH))
        return 0;
    int16_t sample = conversion[ch];
    sample = (sample < 0) ? 0 : sample;
    return sample;
}

float adc_read_volts(adc_channel_e ch)
{
    float raw = (float)adc_read(ch);
    //convert to volts on ADC pin
    float vadc = ((raw*2.8)/4096.0);

    return vadc;
}

uint16_t adc_nsamples(void)
{
    return NSAMP;
}

uint16_t adc_get_samples(adc_channel_e ch, uint16_t *buffer, uint16_t bufflen)
{
    if(!configured)
        return 0;
    uint8_t i, ix = (uint8_t)ch;
    uint16_t samp2copy = (bufflen > NSAMP) ? NSAMP : bufflen;
    for(i = 0; i < samp2copy; i++, buffer++)
    {
        *buffer = (conversion[ix] < 0) ? 0 : conversion[ix];
        ix += MAX_ADC_CH;
    }
    return samp2copy;
}

#if ADC_MODE == MODE_RTC
static void irq_rtc(nrf_drv_rtc_int_type_t int_type)
{
    nrf_drv_rtc_counter_clear(&rtc);
    nrf_drv_rtc_cc_set(&rtc, RTC_CHANNEL, RTC_CC_TIME, true);
    config_adc();
    nrf_drv_saadc_buffer_convert(adc_buffer, ADC_SAMPLES);
    	nrf_drv_saadc_sample();
    }
#endif

static void saadc_callback(nrf_drv_saadc_evt_t const *p_event)
{
#if DEVKIT_NRF52 == 1 //running code using PCA10040 development kit
    /* simulation data for adc raw data for booster  voltage */
    #define VOL_RAW_LOW    1462   //the adc raw data corresponding to 1v
    conversion[ADC_VHEATER]=VOL_RAW_LOW;       //raw data for booster voltage
#else
    memcpy(conversion, adc_buffer, ADC_SAMPLES*sizeof(adc_buffer[0]));
#endif //DEVKIT_NRF52

#if ADC_MODE == MODE_RTC
    nrf_drv_saadc_uninit();
#endif
    NVIC_ClearPendingIRQ(SAADC_IRQn);
    adc_finished=true;
}
