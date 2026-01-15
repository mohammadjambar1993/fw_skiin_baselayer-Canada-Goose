/*
 * autotempctl.h
 *
 *  Created on: Oct. 15, 2021
 *      Author: tmg
 */

#ifndef SRC_AUTOTEMPCTL_H_
#define SRC_AUTOTEMPCTL_H_

#include "../apptypes.h"
#include "../heat_ctrl.h"

#define TEMP_CONTROL_PI_1	1

#define TEMP_CTL_ALGORITHM	TEMP_CONTROL_PI_1

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

app_status_t tctl_init(void);
app_status_t tctl_run(const channel_ctl_t *channels);
app_status_t tctl_get_result(cmd_heat_params_t *params);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_AUTOTEMPCTL_H_ */
