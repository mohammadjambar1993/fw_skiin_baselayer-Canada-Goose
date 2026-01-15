/*
 * hal_clock.h
 *
 *  Created on: Jul 26, 2016
 *      Author: Myant
 */

#ifndef SOURCE_HAL_HAL_CLOCK_H_
#define SOURCE_HAL_HAL_CLOCK_H_

#include "hal_config.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#if (UC_ID == UC_MK22FX512)
#include "fsl_clock.h"
typedef enum
{
    CLKSRC_CORE = kCLOCK_CoreSysClk,
    CLKSRC_BUS1 = kCLOCK_BusClk,
    CLKSRC_BUS2 = kCLOCK_BusClk,
}ClkSrc;
#endif //UC_MK22FX512
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833)
typedef enum
{
    CLKSRC_CORE = 0,    //HCLK64MHz
    CLKSRC_BUS1 = 1,    //PCLK32MHz
    CLKSRC_BUS2 = 2,    //PCLK16MHz
    CLKSRC_BUS3 = 3,    //PCLK1MHz
    CLKSRC_BUS4 = 4,    //PCLK32KHz
}ClkSrc;
#endif //UC_NRF52832

typedef enum
{
    CLKST_OK    = 0,
    CLKST_ERROR = 1
}ClkState;

ClkState hal_clk_init(void);
uint32_t hal_clk_getFrequency(ClkSrc src);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SOURCE_HAL_HAL_CLOCK_H_ */
