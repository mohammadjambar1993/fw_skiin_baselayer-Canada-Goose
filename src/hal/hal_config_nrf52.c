/*
 * hal_config.c
 *
 *  Created on: Sep 13, 2016
 *      Author: Myant
 */

#include "hal_config.h"
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833)

#include "system_nrf52.h"

void hal_config_init(void)
{
    SystemInit();
}

#endif
