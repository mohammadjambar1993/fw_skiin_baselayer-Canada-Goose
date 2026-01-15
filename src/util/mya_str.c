/*
 * mya_str.c
 *
 *  Created on: Sep 21, 2017
 *      Author: Myant
 */

#include <stdlib.h>
#include <string.h>
#include "mya_str.h"
#include "FreeRTOS.h"

bool str_2int(char *str, int64_t *num, int base)
{
    char *end = NULL;
    *num = strtol(str, &end, base);
    return ((*end) == '\0');
}

bool str_2uint(char *str, uint64_t *num, int base)
{
    char *end = NULL;
    *num = strtoul(str, &end, base);
    return ((*end) == '\0');
}

bool str_equal(char *s1, char *s2, uint16_t maxlength)
{
    int r = strncmp(s1, s2, maxlength);
    return (0 == r);
}

bool int_2str(uint32_t num, char *buffer, uint32_t size, int base)
{
    int32_t ret = -1;
    if((NULL == buffer) || (size <= 1))
        return false;
    if(10 == base)
        ret = snprintf(buffer, size, "%ld", num);
    return ((ret > 0) && (ret < size));
}

uint16_t str_copy(char *origin, char *destiny, uint16_t maxlength)
{
    uint16_t len = 0;
    uint16_t max = maxlength;
    char *orig = origin;
    char *dest = destiny;

    while((*orig != 0) && (len < max))
    {
        *dest = *orig;
        dest++;
        orig++;
        len++;
    }
    return len;
}

uint16_t str_length(const char *str, uint16_t max_length)
{
    uint16_t nchars = 0;
    while((*str != 0) && (nchars <= max_length))
    {
        str++;
        nchars++;
    }
    if(nchars > max_length)
        nchars = 0;
    return nchars;
}
