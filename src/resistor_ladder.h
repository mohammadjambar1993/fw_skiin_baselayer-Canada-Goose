/*
 * resistor_ladder.h
 *
 *  Created on: Aug. 2, 2021
 *      Author: tmg
 */

#ifndef SRC_RESISTOR_LADDER_H_
#define SRC_RESISTOR_LADDER_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include "apptypes.h"
#include "hal/hal_gpio.h"

app_status_t rladder_init(const IOPin *outputs, uint8_t n_resistors);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_RESISTOR_LADDER_H_ */
