/*
 * myaqueue.h
 *
 *  Created on: Oct 19, 2016
 *      Author: Myant
 */

#ifndef SRC_UTIL_MYAQUEUE_H_
#define SRC_UTIL_MYAQUEUE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    uint8_t *array;
    uint16_t max;
    uint16_t size_e;
    uint16_t count;
    uint16_t next;
}MyaQueue;

bool myaq_init(MyaQueue *q, void *array, uint16_t len, uint16_t item_len);
bool myaq_enqueue(MyaQueue *q, void *data);
bool myaq_dequeue(MyaQueue *q, void *data);
bool myaq_is_full(MyaQueue *q);
bool myaq_is_empty(MyaQueue *q);
void myaq_clear(MyaQueue *q);
uint16_t myaq_free_positions(MyaQueue *q);
uint16_t myaq_count_items(MyaQueue *q);

#endif /* SRC_UTIL_MYAQUEUE_H_ */
