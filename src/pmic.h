/*
 * pmic.h
 *
 *  Created on: Jan 5, 2017
 *      Author: Rob
 */

#ifndef PMIC_H_
#define PMIC_H_

#include <stdint.h>
#include <stddef.h>
#include "tps65987.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

bool pmic_init(void);
uint8_t  pmic_get_battery_range(void);
uint16_t pmic_get_battery_level(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* PMIC_H_ */
