/*
 * myaqueue.c
 *
 *  Created on: Oct 19, 2016
 *      Author: Myant
 */

#include "myaqueue.h"
#include <string.h>

static uint8_t *get_item(MyaQueue *q, uint16_t index);

bool myaq_init(MyaQueue *q, void *array, uint16_t len, uint16_t item_len)
{
    if(q == NULL || array == NULL || len == 0 || item_len == 0)
        return false;
    q->count = 0;
    q->max = len;
    q->size_e = item_len;
    q->array = array;
    q->next = 0;
    return true;
}

void myaq_clear(MyaQueue *q)
{
    if(q != NULL)
    {
        q->count = 0;
        q->next = 0;
    }
}

static uint8_t *get_item(MyaQueue *q, uint16_t index)
{
    uint8_t *p = NULL;
    p = (q->array)+(index*q->size_e);
    return p;
}

bool myaq_enqueue(MyaQueue *q, void *data)
{
    if(NULL == q || myaq_is_full(q))
        return false;

    uint8_t *p = get_item(q, q->next);
    memcpy(p, data, q->size_e);
    q->count++;
    q->next++;
    if(q->next >= q->max)
        q->next = 0;

    return true;
}

bool myaq_dequeue(MyaQueue *q, void *data)
{
    if(NULL == q || myaq_is_empty(q))
        return false;
    uint16_t ix = ((q->next)-(q->count)+(q->max))%q->max;
    uint8_t *p = get_item(q, ix);
    memcpy(data, p, q->size_e);
    memset(p, 0, q->size_e);
    q->count--;
    return true;
}

bool myaq_is_empty(MyaQueue *q)
{
    if(NULL == q)
        return 0;
    return (q->count == 0);
}

bool myaq_is_full(MyaQueue *q)
{
    if(NULL == q)
        return 0;
    return (q->count == q->max);
}

uint16_t myaq_free_positions(MyaQueue *q)
{
    if(NULL == q)
        return 0;
    return q->max-q->count;
}

uint16_t myaq_count_items(MyaQueue *q)
{
    if(NULL == q)
        return 0;
    return q->count;
}
