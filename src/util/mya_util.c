/*
 * cli_common.c
 *
 *  Created on: Aug 26, 2016
 *      Author: Myant
 */

#include <stdlib.h>
#include <util/mya_util.h>
#include "FreeRTOS.h"

//[[todo]] improve this code to count any byte length
uint16_t util_byteslen(uint64_t num)
{
    uint16_t nbytes = 0;
    if(num <= 0xFF)
        nbytes = 1;
    else if(num <= 0xFFFF)
        nbytes = 2;
    else if(num <= 0xFFFFFF)
        nbytes = 3;
    else if(num <= 0xFFFFFFFF)
        nbytes = 4;
    return nbytes;
}

void util_change_endianness(void *data, uint16_t len)
{
    uint8_t tmp;
    uint8_t *p = (uint8_t*)data;
    size_t l, h;
    for (l=0, h=len-1; h > l; l++, h--)
    {
        tmp = p[l];
        p[l] = p[h];
        p[h] = tmp;
    }
}

//@64MHz a count of 1 is ~188ns
#define MAX_TIME_US 53333000
bool util_blocking_delay_us(uint32_t count)
{
    if(count > MAX_TIME_US)
        return false;
    count *= 6;
    volatile uint32_t i;
    for(i = 0; i < count; i++)
        __NOP();
    return true;
}

bool util_blocking_delay_ms(uint32_t count)
{
    return util_blocking_delay_us(count*1000);
}

bool util_check_flag_us(volatile bool *flag, uint32_t timeout_us)
{
    if((timeout_us > MAX_TIME_US) || (NULL == flag))
        return false;
    bool initial = *flag;
    timeout_us *= 6;
    volatile uint32_t i;
    for(i = 0; i < timeout_us; i++)
    {
        if(*flag != initial)
            break;
    }
    return (*flag != initial);
}

bool util_check_flag_ms(bool *flag, uint32_t timeout_ms)
{
    return util_check_flag_us(flag, timeout_ms*1000);
}

