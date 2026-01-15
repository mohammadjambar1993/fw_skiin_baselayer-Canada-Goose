/*
 * ble_commands.h
 *
 *  Created on: Oct. 4, 2021
 *      Author: tmg
 */

#ifndef SRC_BLE_COMMANDS_H_
#define SRC_BLE_COMMANDS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "apptypes.h"
#include "appconfig.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

app_status_t blecmd_init(void);
void blecmd_process(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_BLE_COMMANDS_H_ */
