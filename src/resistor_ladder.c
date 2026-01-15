/*
 * resistor_ladder.c
 *
 *  Created on: Aug. 2, 2021
 *      Author: tmg
 */

#include "resistor_ladder.h"

app_status_t rladder_init(const IOPin *outputs, uint8_t n_resistors)
{
	if(NULL == outputs || n_resistors == 0)
		return APPST_INVALID_PARAM;
	for(uint8_t i = 0; i < n_resistors; i++)
	{
		hal_gpio_write(outputs[i], 0);
	}
	return APPST_SUCCESS;
}

