/*
 * maf.h
 *	Declaration of moving average filter data structure (in int16)
 *
 *  Created on: Jan 10, 2018
 *      Author: Dingchen (David)
 */
#pragma once
#ifndef SRC_UTIL_MAF_H_
#define SRC_UTIL_MAF_H_

#include <stdbool.h>
#include <stdint.h>

/*
 * Moving average filter data structure (in UINT16)
 */
typedef struct
{
	int16_t* buff;		// fixed length buffer pointer
	int16_t size;		// fixed size
	int16_t head;		// first element index in buffer
	int16_t tail;		// last element index in buffer
	float avg;			// average value
} maf_t;

/*
 * Operations
 */
bool maf_init(maf_t* maf, int16_t* array, int16_t capacity, int16_t init_val);
void maf_pushback(maf_t* maf, int16_t v_in);
float maf_get_avg(maf_t* maf);


/*
 * Moving average filter that is capable to update max/min value within buffer
 */
typedef struct
{
	int16_t* buff;	// fixed length buffer pointer
	int16_t size;	// fixed size
	int16_t head;	// first element index in buffer
	int16_t tail;	// last element index in buffer
	int16_t max;	// maximum value within buffer
	int16_t min; 	// minimum value within buffer
	float   avg;	// moving average
} maf_int16_t;

bool maf_int16_init(maf_int16_t* maf, int16_t* buff, int16_t capacity, int16_t init_v);
void maf_int16_push(maf_int16_t* maf, int16_t v_in);
int16_t maf_int16_get_max(maf_int16_t* maf);
int16_t maf_int16_get_min(maf_int16_t* maf);
float maf_int16_get_avg(maf_int16_t* maf);

#endif /* SRC_UTIL_MAF_H_ */
