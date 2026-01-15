/*
 * 	\file 		uartshell.h
 *  \author 	Tiago Machado Gabardo
 */

#ifndef UARTSHELL_H_
#define UARTSHELL_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void shell_init(void);
void shell_reset(void);
void shell_markRead(void);
bool shell_newCommand(void);
uint8_t shell_getArgs(int8_t ***args);
int8_t *shell_newChar(int8_t c);
int8_t *shell_getPrompt(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* UARTSHELL_H_ */
