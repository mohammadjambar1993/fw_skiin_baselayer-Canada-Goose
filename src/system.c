/*
 * system.c
 *
 *  Created on: Aug 26, 2019
 *      Author: Myant
 */

#include <string.h>
#include "appconfig.h"
#include "system.h"
#include "ble_rpc.h"
#include "ble_evt.h"
#include "logger.h"
#include "logio.h"
#include "tskctrl.h"
#include "heater.h"
#include "heat_ctrl.h"
#include "ble_commands.h"
#include "bandcfg.h"

#define _print(...)  					\
do                      				\
{                       				\
    SEGGER_RTT_SetTerminal(0);        	\
    SEGGER_RTT_printf(0, __VA_ARGS__);  \
}while(0)

#define ENABLE_LOGGER_SYSTEM
#ifdef ENABLE_LOGGER_SYSTEM
	#include "util/logger.h"
	#define _debug 		log_debug
	#define _info		log_info
	#define _warn		log_warn
	#define _error		log_error
#else
	#define _debug
	#define _info
	#define _warn
	#define _error
#endif


/* led indications  */
#define BLINK_STANDBY   	1
#define BLINK_CONNECTED     2
#define STATE_COLOR     	COLOR_BLUE
#define BLINK_INVALID  		0xFF

static void set_pod_led_color(void);
static void tsk_system(void *params);

/* Timer to update BLE characteristics 0x6833*/
static TimerHandle_t timer_ble = NULL;
static inline void create_ble_timer(void);
static void irq_timer_ble(TimerHandle_t xtimer);

static int8_t state_led = -1;

void sys_init(void)
{
	state_led = logio_blink(STATE_COLOR, BLINK_STANDBY);
	ASSERT_TASK(tsk_create(TSK_SYSTEM, tsk_system));
}

static void tsk_system(void *params)
{
    EventBits_t events, evt_leds, triggered;
    events = EVT_BLE_COMMAND | EVT_BLE_HEAT_INFO | EVT_MOD_UPDATE_INFO;
    evt_leds = EVT_BLE_CONNECTED | EVT_BLE_DISCONNECTED | EVT_HARDWARE_FAIL;

    create_ble_timer();
    if(!band_init())
    {
    	_error("[sys] failed to initialize band parameters");
    }
    if(APPST_SUCCESS != heat_init())
    {
    	_error("[sys] failed to initialize the heat module");
    }
    while(1)
    {
		triggered = os_evt_wait(events, pdMS_TO_TICKS(3000));
		os_evt_clear(events);
        if(EVT_BLE_COMMAND & triggered)
        {
        	blecmd_process();
        }
        if(evt_leds & triggered)
        {
            set_pod_led_color();
        }
        if(EVT_BLE_HEAT_INFO & triggered)
        {
        	ble_update_heatinfo(true);
        }
		if(EVT_MOD_UPDATE_INFO & triggered)
        {
        	ble_update_modinfo(true);
        }
    }
}

static void set_pod_led_color(void)
{
	static uint8_t current_color = COLOR_BLUE;
	static uint8_t current_blinks = BLINK_STANDBY;
	uint8_t new_color;
	uint8_t nblinks;

	//blink count is based on BLE connection status
	nblinks = ble_is_connected() ? BLINK_CONNECTED : BLINK_STANDBY;
	//blink color is based on failure mode
	new_color = heat_is_hardware_failed() ? COLOR_RED : COLOR_BLUE;
	//if # of blinks or color changed, update the logio module
	if((nblinks != current_blinks) || (new_color != current_color))
	{
		current_blinks = nblinks;
		current_color = new_color;
		logio_change(state_led, current_color, nblinks);
	}
}

static void irq_timer_ble(TimerHandle_t xtimer)
{
	os_evt_trigger(EVT_BLE_HEAT_INFO);
}

static inline void create_ble_timer(void)
{

	timer_ble = xTimerCreate("tmble",pdMS_TO_TICKS(1000),pdTRUE,0,
								 irq_timer_ble);
	if(NULL == timer_ble)
	{
		diag_inc_flag(FLG_MTU_TIMER_CREATE);
		_warn("Failed to create MTU timer");
		return;
	}
    if(pdPASS != xTimerStart(timer_ble, 0))
    {
    	diag_inc_flag(FLG_MTU_TIMER_START);
    	_warn("Failed to initialize MTU timer");
    }
}


