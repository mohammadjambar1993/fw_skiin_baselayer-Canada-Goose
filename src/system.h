/*
 * system.h
 *
 *  Created on: Aug 26, 2019
 *      Author: Myant
 */

#ifndef SRC_SYSTEM_H_
#define SRC_SYSTEM_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cmd_protocol.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

void sys_init(void);
void sys_cycle_printing(void); //<<hack>> function used only for debug

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_SYSTEM_H_ */
