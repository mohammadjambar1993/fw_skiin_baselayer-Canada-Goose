/*
 * cli_tests.h
 *
 *  Created on: Aug 24, 2016
 *      Author: Myant
 */

#ifndef SRC_CLI_MX25R6435_H_
#define SRC_CLI_MX25R6435_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include "cli.h"

uint16_t cli_mx2564_get_commands(const CliCommand **commands);
void cli_mx2564_start_profile(void);
void cli_mx2564_pwr_down(void);
void cli_mx2564_pwr_up(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_CLI_MX25R6435_H_ */
