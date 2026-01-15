/*
 * logpin.h
 *
 *  Created on: Sep 20, 2016
 *      Author: Myant
 */

#ifndef SRC_UTIL_LOGIO_H_
#define SRC_UTIL_LOGIO_H_

#include "hal_gpio.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#define _RED        1
#define _GREEN      2
#define _BLUE       4
#define INVALID_LED -1

typedef enum
{
	 	COLOR_BLACK     = 0,
	    COLOR_RED       = _RED,
	    COLOR_GREEN     = _GREEN,
	    COLOR_BLUE      = _BLUE,
	    COLOR_YELLOW    = _GREEN+_RED,
	    COLOR_MAGENTA   = _RED+_BLUE,
	    COLOR_CYAN      = _GREEN+_BLUE,
	    COLOR_WHITE     = _RED+_GREEN+_BLUE,
}color_t;

void logio_init(void);
void logio_enable_task(bool en);
int8_t logio_blink(color_t color, uint8_t blinks);
int8_t logio_change(int8_t index, color_t color, uint8_t blinks);
bool logio_is_enabled(void);
void logio_set_color(color_t color);
void logio_set_leds(uint8_t red, uint8_t green, uint8_t blue);


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_UTIL_LOGIO_H_ */
