/*
 * evthandlers.c
 *
 *  Created on: Oct 26, 2016
 *      Author: Myant
 */

#include "evthandlers.h"
#include <string.h>

#define INVALID_IX  -1

static inline void *get_item(HandlerArray *a, uint16_t index);
static int16_t get_index(HandlerArray *a, void *item, int16_t *ixfree);

bool hndl_init(HandlerArray *a, void *callbacks, uint16_t scallb, uint16_t max)
{
    if(NULL == a || NULL == callbacks || max == 0 || scallb == 0)
        return false;
    a->array = callbacks;
    a->size = scallb;
    a->max = max;
    a->num = 0;
    //init array
    memset(callbacks, 0, max*scallb);
    return true;
}

static int16_t get_index(HandlerArray *a, void *item, int16_t *ixfree)
{
    uint16_t i;
    uint32_t *p;

    if(NULL != ixfree)
        *ixfree = INVALID_IX;

    for(i = 0; i < a->max; i++)
    {
        p = get_item(a, i);
        if(*ixfree == INVALID_IX && *p == 0)
            *ixfree = i;
        if((uint32_t*)*p == item)
            return i;
    }
    return INVALID_IX;
}

static inline void *get_item(HandlerArray *a, uint16_t index)
{
    void *p = (a->array)+(a->size*index);
    return p;
}

bool hndl_add(HandlerArray *a, void *callback)
{
    if(NULL == a || NULL == callback || hndl_is_full(a))
        return false;

    int16_t ix, ix_free;
    ix = get_index(a, callback, &ix_free);
    if(INVALID_IX != ix) //callback already present
        return true;

    void *p = get_item(a, ix_free);
    memcpy(p, &callback, a->size);
    a->num++;
    return true;
}

bool hndl_remove(HandlerArray *a, void *callback)
{
    if(NULL == a || NULL == callback || hndl_is_empty(a))
        return false;

    int16_t ix;
    ix = get_index(a, callback, NULL);
    if(INVALID_IX == ix) //no callback to remove
        return false;

    uint32_t *p = get_item(a, ix);
    memset(p, 0, a->size);
    a->num--;
    return true;
}

bool hndl_is_full(HandlerArray *a)
{
    if(NULL == a)
        return false;
    return (a->max == a->num);
}

bool hndl_is_empty(HandlerArray *a)
{
    if(NULL == a)
        return false;
    return (a->num == 0);
}

uint16_t hndl_get_valid_callbacks(HandlerArray *a)
{
    if(NULL == a)
        return 0;
    return a->num;
}
