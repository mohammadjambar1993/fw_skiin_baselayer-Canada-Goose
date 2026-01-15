/*
 * ringbuff.h
 *
 *  Circular buffer module
 *
 *  Circular (ring) buffer data structure and its implementation
 *
 *  Created on: Oct 24, 2017
 *      Author: Dingchen (David)
 */
#pragma once
#ifndef SRC_UTIL_RINGBUFF_H_
#define SRC_UTIL_RINGBUFF_H_

#include <stdbool.h>
#include <stdint.h>

#define RB_MAX	  100000000

/**
 * circular buffer structure definition
 */
typedef struct
{
	float* buff;     // pointed to the fixed length array
	float  max;      // maximum value within the buffer
	float  min;      // minimum value within the buffer
	int16_t capacity;// capacity of the buffer (no larger than 32,767)
	int16_t size;    // size of the buffer, i.e. the number of used elements
	int16_t head;    // index of the first element in buffer (frontend)
	int16_t tail;    // index of the last element in buffer (backend)
} rb_t;

bool rb_init(rb_t* rb, float* array, int16_t capacity);
bool rb_isfull(rb_t* rb);
bool rb_isempty(rb_t* rb);
bool rb_get(rb_t* rb, int16_t offset, float* v_out);
bool rb_pushback(rb_t* rb, float v_in, float* v_out, bool flag);
bool rb_popfront(rb_t* rb, float* v_out);
void rb_free(rb_t* rb);
bool rb_set_maxmin(rb_t* rb, float max, float min);
bool rb_set_headtail(rb_t* rb, int16_t h, int16_t t);

// ===========================================
// Circular Buffer to store int16 type of data
// ===========================================
typedef struct
{
	int16_t* buff;		// pointed to the fixed length array
	int16_t max;		// maximum value within the buffer
	int16_t min;		// minimum value within the buffer
	int16_t capacity;	// capacity of the buffer
	int16_t size;		// size of the circular buffer
	int16_t head;		// index of the first element in buffer
	int16_t tail;		// index of the last element in buffer
} rb_int16_t;

bool rb_int16_init(rb_int16_t* rb, int16_t* buff, int16_t capacity);
bool rb_int16_push(rb_int16_t* rb, int16_t v_in, int16_t* v_out, bool flag);
bool rb_int16_pop(rb_int16_t* rb, int16_t* v_out);
bool rb_int16_get(rb_int16_t* rb, int16_t offset, int16_t* v_out);
bool rb_int16_isfull(rb_int16_t* rb);
bool rb_int16_isempty(rb_int16_t* rb);
void rb_int16_free(rb_int16_t* rb);

// ===========================================
// Circular Buffer to store int32 type of data
// ===========================================
typedef struct
{
	int32_t* buff;
	int32_t max;
	int32_t min;
	int16_t capacity;
	int16_t size;
	int16_t head;
	int16_t tail;
} rb_int32_t;

bool rb_int32_init(rb_int32_t* rb, int32_t* buff, int16_t capacity);
bool rb_int32_push(rb_int32_t* rb, int32_t v_in, int32_t* v_out, bool flag);
bool rb_int32_pop(rb_int32_t* rb, int32_t* v_out);
bool rb_int32_get(rb_int32_t* rb, int16_t offset, int32_t* v_out);
bool rb_int32_isfull(rb_int32_t* rb);
bool rb_int32_isempty(rb_int32_t* rb);
void rb_int32_free(rb_int32_t* rb);

#endif /* SRC_UTIL_RINGBUFF_H_ */
