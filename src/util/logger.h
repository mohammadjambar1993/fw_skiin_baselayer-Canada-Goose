/*
 * logger.h
 *
 *  Created on: Sep 19, 2016
 *      Author: Myant
 */

#ifndef SRC_LOGGER_H_
#define SRC_LOGGER_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */
#include <stdbool.h>
#include "appconfig.h"
#include "SEGGER_RTT.h"

typedef enum
{
    LEVEL_DEBUG = 1,
    LEVEL_INFO  = 2,
    LEVEL_WARN  = 3,
    LEVEL_ERROR = 4,
}LogLevel;

typedef enum
{
    LOG_SEGGER_RTT  = 1
}LogOutput;

void log_init(LogLevel level, LogOutput out);
void log_debug(char *msg, ...);
void log_info(char *msg, ...);
void log_warn(char *msg, ...);
void log_error(char *msg, ...);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_LOGGER_H_ */
