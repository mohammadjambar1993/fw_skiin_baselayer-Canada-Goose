/*
 * cli.h
 *
 *  Created on: Aug 23, 2016
 *      Author: Myant
 */

#ifndef SRC_CLI_H_
#define SRC_CLI_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "SEGGER_RTT.h"
#include "appconfig.h"

//############################# domains definitions ###########################
//DM_xx constants must reflect the index inside domains array
#define DM_XGA          0

#define SEGGER_OUT_BUFFER       0   //segger output buffer
#define SEGGER_CLI_TERMINAL     1   //RTT terminal used by CLI
#define cli_print(...)  \
do                      \
{                       \
    SEGGER_RTT_SetTerminal(SEGGER_CLI_TERMINAL);        \
    SEGGER_RTT_printf(SEGGER_OUT_BUFFER, __VA_ARGS__);  \
}while(0)
#define INVALID_ARGS    "Invalid arguments"
#define NEW_LINE        "\r\n"

#define check_args(n,min)               \
if(n < min)                             \
{                                       \
    cli_print(NEW_LINE INVALID_ARGS);   \
    return;                             \
}

typedef struct
{
    const char *name;
    const char *help;
    void (*func)(char *argv[], uint8_t argc);
}CliCommand;

void cli_init(void);
void cli_printargs(char *argv[], uint8_t argc);
void cli_exec_cmd(uint8_t domain, char *argv[], uint8_t argc);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_CLI_H_ */
