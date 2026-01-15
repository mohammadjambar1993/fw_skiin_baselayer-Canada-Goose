/*
 * hal_timer.h
 *
 *  Created on: Jul 28, 2016
 *      Author: Myant
 */

#ifndef SOURCE_HAL_HAL_TIMER_H_
#define SOURCE_HAL_HAL_TIMER_H_

#include "hal_config.h"
#include "hal_clock.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#if UC_ID == UC_MK22FX512
#include "fsl_pit.h"
typedef enum
{
    TIMER1 = kPIT_Chnl_0,
    /*TIMER2 = kPIT_Chnl_1,
    TIMER3 = kPIT_Chnl_2,
    TIMER4 = kPIT_Chnl_3,*/
}TmrID;
#endif //UC_ID == MK22FX512

#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833)
typedef enum
{
    TIMER1 = 0,
    TIMER2 = 1,
    TIMER3 = 2,
    TIMER4 = 3,
}TmrID;
#endif //UC_ID == UC_NRF52832

typedef enum
{
    TMST_CLK_OK,
    TMST_CLK_SRC_ERROR,
    TMST_CLK_NO_REDEFINITION,
    TMST_ALREADY_CONFIGURED,
    TMST_NOT_CONFIGURED,
    TMST_CALLB_OK,
    TMST_CALLB_FULL,
    TMST_CALLB_INVALID,
    TMST_CALLB_EXISTENT,
    TMST_CALLB_INVALID_TIME,
    TMST_ID_INVALID,
    TMST_OP_OK,
    TMST_OP_FAIL,
}TmrStatus;

TmrStatus hal_tmr_init(TmrID id, ClkSrc clk, uint32_t us);
TmrStatus hal_tmr_addCallBack(TmrID id, uint32_t us, void(*l)(TmrID));
TmrStatus hal_tmr_removeCallBack(TmrID id, void(*l)(TmrID));
TmrStatus hal_tmr_enableCallBack(TmrID id, void(*l)(TmrID), uint8_t en);
TmrStatus hal_tmr_start(TmrID id);
TmrStatus hal_tmr_stop(TmrID id);
TmrStatus hal_tmr_enableIRQ(TmrID id, uint8_t en);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SOURCE_HAL_HAL_TIMER_H_ */
