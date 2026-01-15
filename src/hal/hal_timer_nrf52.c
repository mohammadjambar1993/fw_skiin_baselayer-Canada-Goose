/*
 * hal_timer_fx512.c
 *
 *  Created on: Jul 28, 2016
 *      Author: Myant
 */

#include "hal_timer.h"
#include "hal_config.h"
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833) && (ENABLE_HAL_TIMER == 1)
#include "nrf_timer.h"
#include "nrf_drv_common.h"
#include "nrf_drv_timer.h"
#include "app_util_platform.h"

//[[config]] set how many callback functions are supported
#ifndef NCALLB_TIMER1
#define NCALLB_TIMER1   3
#endif

typedef struct
{
    uint8_t en;
    uint32_t ct;
    uint32_t overflow;
    void (*callb)(TmrID);
}Listener;

bool configured = false;
static uint32_t ticks = 1;
static Listener list_t1[NCALLB_TIMER1] = {{0}};

static Listener *findListener(void(*l)(TmrID));

static const nrf_drv_timer_t timer1 =
{
    .p_reg = NRF_TIMER0,    //timer 1 register base
    .instance_id = 0,       //timer's index inside driver structures
    .cc_channel_count = 4   //timer1 has 4 channels
};

static const nrf_drv_timer_config_t cnf_timer1 =
{
    .frequency = NRF_TIMER_FREQ_16MHz,
    .mode = NRF_TIMER_MODE_TIMER,
    .bit_width = NRF_TIMER_BIT_WIDTH_32,
    .interrupt_priority = APP_IRQ_PRIORITY_LOW,
    .p_context = NULL //argument passed to listener
};

TmrStatus hal_tmr_init(TmrID id, ClkSrc clk, uint32_t us)
{
    ret_code_t err; //0 == success

    if(configured)
        return TMST_ALREADY_CONFIGURED;

    err = nrf_drv_timer_init(&timer1, &cnf_timer1, NULL);
    ticks = nrf_drv_timer_us_to_ticks(&timer1, us);
    nrf_drv_timer_extended_compare(&timer1, NRF_TIMER_CC_CHANNEL0, ticks,
            NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, false);

    if(err)
        return TMST_OP_FAIL;

    configured = true;
    return TMST_OP_OK;
}

TmrStatus hal_tmr_addCallBack(TmrID id, uint32_t us, void (*l)(TmrID))
{
    uint32_t counter;
    Listener *freep;

    //is timer configured?
    if(!configured)
        return TMST_NOT_CONFIGURED;
    //listener is valid?
    if(NULL == l)
        return TMST_CALLB_INVALID;
    //requested time is valid?
    counter = nrf_drv_timer_us_to_ticks(&timer1, us);
    if(counter < ticks)
        return TMST_CALLB_INVALID_TIME;
    //maybe the listener was already included
    if(NULL != findListener(l))
        return TMST_CALLB_EXISTENT;
    //or the callback array is full
    freep = findListener(NULL);
    if(NULL == freep)
        return TMST_CALLB_FULL;
    //everything is fine. Setup new listener.
    freep->callb = l;
    freep->ct = 0;
    freep->en = 0;
    freep->overflow = counter/ticks;

    return TMST_CALLB_OK;
}

TmrStatus hal_tmr_removeCallBack(TmrID id, void (*l)(TmrID))
{
    Listener *listener;

    //is timer configured?
    if(!configured)
        return TMST_NOT_CONFIGURED;
    //listener is valid?
    if(NULL == l)
        return TMST_CALLB_INVALID;
    listener = findListener(l);
    if(NULL != listener)
        listener->callb = NULL;
    return TMST_OP_OK;
}

TmrStatus hal_tmr_enableCallBack(TmrID id, void (*l)(TmrID), uint8_t en)
{
    Listener *list;
    list = findListener(l);
    if(NULL == list)
        return TMST_CALLB_INVALID;
    list->en = en;
    return TMST_OP_OK;
}

TmrStatus hal_tmr_start(TmrID id)
{
    nrf_drv_timer_enable(&timer1);
    return TMST_OP_OK;
}

TmrStatus hal_tmr_stop(TmrID id)
{
    nrf_drv_timer_disable(&timer1);
    return TMST_OP_OK;
}

TmrStatus hal_tmr_enableIRQ(TmrID id, uint8_t en)
{
    if(en)
        nrf_drv_timer_compare_int_enable(&timer1, NRF_TIMER_CC_CHANNEL0);
    else
        nrf_drv_timer_compare_int_disable(&timer1, NRF_TIMER_CC_CHANNEL0);
    return TMST_OP_OK;
}

static Listener *findListener(void(*l)(TmrID))
{
    uint8_t i;
    Listener *list = NULL;
    Listener *array = list_t1;

    for(i = 0; i < NCALLB_TIMER1; i++, array++)
    {
        if(array->callb == l)
        {
            list = array;
            break;
        }
    }
    return list;
}

//void TIMER0_IRQHandler(void)
//{
//    uint8_t i;
//    nrf_timer_event_t event;
//    Listener *l = list_t1;
//
//    //clear interrupt flag
//    event = nrf_timer_compare_event_get(NRF_TIMER_CC_CHANNEL0);
//    nrf_timer_event_clear(timer1.p_reg, event);
//    //warn listeners
//    for(i = 0; i < NCALLB_TIMER1; i++, l++)
//    {
//        if(l->en)
//        {
//            l->ct++;
//            if(l->ct >= l->overflow)
//            {
//                l->ct = 0;
//                l->callb(TIMER1);
//            }
//        }
//    }
//}



#endif //UC_ID == UC_NRF52832
