/*
 * adc_ctrl.h
 *
 *  Created on: Oct 11, 2017
 *      Author: Myant
 */

#ifndef SRC_ADC_CTRL_H_
#define SRC_ADC_CTRL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sensors.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    ADC_VHEATER , 		//boost converter output
    MAX_ADC_CH,
	ADC_INVALID = 0xFF	//invalid/unused channel
}adc_channel_e;


void adc_init(void);
void adc_uninit(void);
void adc_start(void);
bool adc_sample(void);
uint8_t adc_get_mode(void);
uint16_t adc_nsamples(void);
uint16_t adc_read(adc_channel_e ch);
float adc_read_volts(adc_channel_e ch);
uint16_t adc_get_samples(adc_channel_e ch, uint16_t *buffer, uint16_t bufflen);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_ADC_CTRL_H_ */
