/*
 * logpin.c
 *
 *  Created on: Sep 20, 2016
 *      Author: Myant
 */

#include <logio.h>
#include "appconfig.h"

#if (ENABLE_IO_LOGGER == 0)
void logio_init(void){}
void logio_enable_task(bool en){}
int8_t logio_blink(color_t color, uint8_t blinks){return 0;}
int8_t logio_change(int8_t index, color_t color, uint8_t blinks){return 0;}
bool logio_is_enabled(void){return false;}
#elif (ENABLE_IO_LOGGER == 1)

#define LED_ON      0
#define LED_OFF     1

#include "FreeRTOS.h"
#include "task.h"
#include "appconfig.h"
#include "tskctrl.h"
#include "hal_config.h"
#include "wdt.h"

//variables and constants used to control led blinking
#define LEN_OUTPUTS     10
#define BASE_TIME       50  //ms
//times
#define TIME_ON         2   //TIME_ON*BASE_TIME == time the output is active
#define TIME_OFF        6   //TIME_OFF*BASE_TIME == time the output is inactive
//TIME_GAP*BASE_TIME == time between two blinking sessions
#define TIME_GAP        25
#define BLINK_FAST      6   //6*BASE_TIME
#define BLINK_SLOW      9   //9*BASE_TIME

/******************************************************
   TIME_ON
     +-+   +-+   +-+                 +-+   +-+   +-+
     | |   | |   | |                 | |   | |   | |
     | |   | |   | |   TIME_GAP      | |   | |   | |
 +---+ +-+-+ +---+ +-----------------+ +---+ +---+ +--
         |
      TIME_OFF
*******************************************************/

typedef struct
{
    color_t color;
    uint8_t nblinks;
    uint8_t ctblinks;
    uint8_t timer;
    uint8_t timeoff;
}SignalIO;

static SignalIO outputs[LEN_OUTPUTS] = {{0}};
static TickType_t last_tick;
static TaskHandle_t hnd;
static bool initialized = false;
static wdt_entry wdt = NULL;

static SignalIO *getFreeIO(int8_t *index);
static void tsk_iolog(void *params);
static void setled(color_t color);

void logio_init(void)
{
    if(!initialized)
    {
        uint8_t i;
        SignalIO *sig = outputs;
        for(i = 0; i < LEN_OUTPUTS; i++, sig++)
        {
            sig->color = COLOR_BLACK;
            sig->timeoff = TIME_OFF;
        }
        hnd = tsk_create(TSK_LIB_IOLOG, tsk_iolog);
        ASSERT_TASK(hnd);
        vTaskSuspend(hnd);
        initialized = true;
        //config watchdog timer
        wdt_config_t wconfig;
        wconfig.id = TSK_LIB_IOLOG;
        wconfig.enabled = false;
        wconfig.event_mask = 0;
        wconfig.timeout = 2;
        wconfig.type = WDT_E_PERIODIC;
        wdt = wdt_config_entry(&wconfig, pdMS_TO_TICKS(200));
    }
}

void logio_enable_task(bool en)
{
    if(NULL == hnd)
        return;
    eTaskState st = tsk_state_by_id(TSK_LIB_IOLOG);
    if(en)
    {
        if(eSuspended == st)
        {
            wdt_enable(wdt, true);
            tsk_resume_by_hnd(hnd);
        }
    }
    else
    {
        if(eSuspended != st)
        {
            wdt_enable(wdt, false);
            tsk_suspend_by_hnd(hnd);
        }
    }
}

void logio_set_leds(uint8_t red, uint8_t green, uint8_t blue)
{
    eTaskState st = tsk_state_by_id(TSK_LIB_IOLOG);
    if(eSuspended == st) //only control LEDs directly if the task is disabled
    {
        hal_gpio_write(LED_R, !red);
        hal_gpio_write(LED_G, !green);
        hal_gpio_write(LED_B, !blue);
    }
}

static SignalIO *getFreeIO(int8_t *index)
{
    uint8_t i;
    SignalIO *sig = outputs;
    for(i = 0; i < LEN_OUTPUTS; i++, sig++)
    {
        *index = i;
        if(sig->nblinks == 0) //free position
            return sig;
    }
    *index = -1;
    return NULL;
}

bool logio_is_enabled(void)
{
    eTaskState tskst = tsk_state_by_id(TSK_LIB_IOLOG);
    return (eRunning == tskst) || (eBlocked == tskst) || (eReady == tskst);
}

int8_t logio_blink(color_t color, uint8_t blinks)
{
    eTaskState state;
    int8_t index = -1;
    SignalIO *sig = getFreeIO(&index);
    //no free position, blinks invalid or pin already being used
    if(NULL == sig || blinks == 0 || color == COLOR_BLACK || !initialized)
        return index;
    sig->color = color;
    sig->nblinks = blinks;
    sig->ctblinks = 0;
    sig->timer = 0;
    if(COLOR_RED == color)
        sig->timeoff = BLINK_SLOW;
    else
        sig->timeoff = BLINK_FAST;

    state = eTaskGetState(hnd);
    if(eSuspended == state)
    {
        last_tick = xTaskGetTickCount();
        wdt_enable(wdt, true);
        vTaskResume(hnd);
    }
    return index;
}

int8_t logio_change(int8_t index, color_t color, uint8_t blinks)
{
    if(index < 0 || index >= LEN_OUTPUTS)
        return -1;
    eTaskState state;
    SignalIO *sig = &outputs[index];
    if(sig->nblinks == 0) //free position (output not used yet)
        return -1;
    sig->color = color;
    sig->nblinks = blinks;
    sig->ctblinks = 0;
    sig->timer = 0;

    state = eTaskGetState(hnd);
    if(eSuspended == state)
    {
        last_tick = xTaskGetTickCount();
        wdt_enable(wdt, true);
        vTaskResume(hnd);
    }
    return index;
}

static void setled(color_t color)
{
    uint8_t r,g,b;
    r = color & _RED;
    g = color & _GREEN;
    b = color & _BLUE;
    hal_gpio_write(LED_R, !r);
    hal_gpio_write(LED_G, !g);
    hal_gpio_write(LED_B, !b);
}

void logio_set_color(color_t color)
{
    if(!logio_is_enabled())
        setled(color);
}

static void tsk_iolog(void *params)
{
    static uint8_t i = 0;
    uint8_t ct;
    SignalIO *sig = outputs;
    const TickType_t period = pdMS_TO_TICKS(BASE_TIME);
    wdt_enable(wdt, true);
    while(1)
    {

    	ct = 0;
        while((sig->nblinks == 0) && (ct < LEN_OUTPUTS))
        {
            if(++i >= LEN_OUTPUTS)
                i = 0;
            sig = &outputs[i];
            ct++;
        }
        if(sig->nblinks == 0)
        {
            wdt_enable(wdt, false);
            vTaskSuspend(hnd);
            wdt_enable(wdt, true);
            goto wait;
        }

        if(sig->ctblinks >= sig->nblinks)
            sig->ctblinks = 0;
        if(sig->timer == 0)
            setled(sig->color);

        sig->timer++;

        if(sig->timer == TIME_ON)
            setled(COLOR_BLACK);
        if(sig->timer == sig->timeoff)
        {
            sig->ctblinks++;
            if(sig->ctblinks < sig->nblinks)
                sig->timer = 0;
        }
        if(sig->timer == TIME_GAP)
        {
            sig->timer = 0;
            if(++i >= LEN_OUTPUTS)
                i = 0;
            sig = &outputs[i];
        }
    wait:
        wdt_feed(wdt);
        vTaskDelayUntil(&last_tick, period);
    }
}

#endif //ENABLE_IO_LOGGER
