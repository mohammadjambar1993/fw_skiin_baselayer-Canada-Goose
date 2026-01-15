/*
 * evthandlers.h
 *
 *  Created on: Oct 26, 2016
 *      Author: Myant
 */

#ifndef SRC_UTIL_EVTHANDLERS_H_
#define SRC_UTIL_EVTHANDLERS_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    void *array;    //array of function handlers
    uint16_t size;  //size in bytes of each array element
    uint16_t num;   //number of valid elements inside array
    uint16_t max;   //number of positions inside array
}HandlerArray;

bool hndl_init(HandlerArray *a, void *callbacks, uint16_t scallb, uint16_t max);
bool hndl_add(HandlerArray *a, void *callback);
bool hndl_remove(HandlerArray *a, void *callback);
bool hndl_is_full(HandlerArray *a);
bool hndl_is_empty(HandlerArray *a);
uint16_t hndl_get_valid_callbacks(HandlerArray *a);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_UTIL_EVTHANDLERS_H_ */
