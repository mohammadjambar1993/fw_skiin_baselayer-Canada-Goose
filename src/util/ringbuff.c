/*
 * ringbuff.c
 *
 * Implementation (definitions) of circular buffer related operations
 *
 *  Created on: Oct 24, 2017
 *      Author: Dingchen (David)
 */
#include "ringbuff.h"
#include "logger.h"
#include <float.h>
#include <stdbool.h>
#include <math.h>

#pragma GCC push_options
#pragma GCC optimize ("O3")

#define EPSILON   	   (1e-5F)
#define MIN_NORMAL     (1e-20F)

/*
 * private functions defined for data structure implementation
 */
static void rb_update_max_traverse(rb_t* rb);
static void rb_update_min_traverse(rb_t* rb);
static void rb_update_max_compare(rb_t* rb, float v_in, float v_out);
static void rb_update_min_compare(rb_t* rb, float v_in, float v_out);
static bool isequalf(float a, float b);

/*
 * Definitions of private and public functions
 */

/**
* @brief: initialization of a ring buffer, pointed to a fixed-length array
* @params:
*	rb [in]: pointer pointed to the ring buffer to be initialized
*	array [in]: address of the fixed-length array for buffer
*	capacity [in]: capacity of ring buffer, should be the same as the length of
*				   the array pointed to by 'array' argument
*/
bool rb_init(rb_t* rb, float* array, int16_t capacity)
{
	if( !rb || !array )
	{
		log_error("[rb_init] Null pointer");
		return false;
	}

	if( capacity <= 0 )
	{
		log_error("[rb_init] Invalid input");
		return false;
	}

	rb->buff = array;

	rb->max = -RB_MAX;
	rb->min = RB_MAX;

	rb->capacity = capacity;
	rb->head = 0;
	rb->size = 0;
	rb->tail = -1;

	return true;
}

/**
* @brief: check if the ring buffer is full
* @params:
*	rb [in]: ring buffer pointer
* @retval: true if the ring buffer is full, otherwise false
*/
bool rb_isfull(rb_t* rb)
{
	if( !rb )
	{
		log_error("[rb_isfull] Null pointer");
		return false;
	}
	return (rb->size==rb->capacity);
}

/**
* @brief: check if the ring buffer is empty
* @params:
*	rb [in]: ring buffer pointer
* @retval: true if the ring buffer is empty, otherwise false
*/
bool rb_isempty(rb_t* rb)
{
	if( !rb )
	{
		log_error("[rb_isempty] Null pointer");
		return false;
	}
	return (rb->size==0);
}

/**
* @brief: get a specified element from the ring buffer given offset
* @params:
*	rb [in]: ring buffer pointer
*	offset [in]: offset from rb->tail, negative means backward
*	v_out [out]: address of the variable to store the popped out element's value
* @retval: true if successfully get the element from buffer, otherwise false
*/
bool rb_get(rb_t* rb, int16_t offset, float* v_out)
{
	// invalid ring buffer pointer or empty buffer
	if(!rb || !rb->buff || rb->size==0)
	{
		log_error("[rb_get] Null pointer or empty");
		return false;
	}

	// invalid offset
	if(-offset >= rb->size || offset >= rb->size)
	{
		log_error("[rb_get] Invalid offset: %d", offset);
		return false;
	}

	int16_t idx = rb->tail + offset;

	if(idx >= rb->capacity)
		idx -= rb->capacity;
	else if(idx < 0)
		idx += rb->capacity;

	*v_out = rb->buff[idx];

	return true;
}

/**
* @brief: push an elment to the backend of the ring buffer
* @params:
*	rb [in]: ring buffer pointer
*	v_in [in]: element value to be pushed into ring buffer
*	v_out [out]: address of the variable to store the popped out element's value
* 	flag [in]: boolean flag to indicate if to update max/min value
* @retval: true if successfully push the new data into buffer, otherwise false
*/
bool rb_pushback(rb_t* rb, float v_in, float* v_out, bool flag)
{
	static float last_valid = 0;

	if(!rb || !rb->buff)
	{
		log_error("[rb_pushback] Null pointer");
		return false;
	}

	// full, then pop 1 element and then push
	if (rb->size == rb->capacity)
	{
		*v_out = rb->buff[rb->head];
		// shifting rb->head
		if (++rb->head >= rb->capacity)
			rb->head = 0;
		--rb->size;
	}

	// updating rb->tail
	if (++rb->tail >= rb->capacity)
		rb->tail = 0;
	// check if NaN or Infinity
	if (isnanf(v_in) || isinff(v_in) || fabsf(v_in)>RB_MAX )
	{
		log_warn("[rb_push] NaN or Inf or Unbnd");
		v_in = last_valid;
	}
	else
		last_valid = v_in;
	rb->buff[rb->tail] = v_in;
	++rb->size;

	// If needed to update max/min
	if (flag)
	{
		rb_update_max_compare(rb, v_in, *v_out);
		rb_update_min_compare(rb, v_in, *v_out);
	}

	return true;
}

/**
* @brief: pop out the element at the front end of ring buffer
* @params:
*	rb [in]: ring buffer pointer
*	v_out [out]: address of the variable to store the popped out element's value
* @retval: true if
*/
bool rb_popfront(rb_t* rb, float* v_out)
{
	// if empty, no pop operation and return false
	if(!rb || rb->size==0 )
	{
		log_debug("[rb_popfront] Null pointer or empty");
		return false;
	}

	*v_out = rb->buff[rb->head];

	// shifting(updating) rb->head
	if (++rb->head >= rb->capacity)
		rb->head = 0;
	--rb->size;

	return true;
}

/**
* @brief: free the memory allocated to the ring buffer
* @params:
*	rb [in]: ring buffer pointer
* @retval: true if successfully pop out the data from buffer, otherwise false
*/
void rb_free(rb_t* rb)
{
	if (rb)
	{
		rb->buff = NULL;
		rb->max = 0;
		rb->min = 0;
		rb->capacity = 0;
		rb->size = 0;
		rb->head = -1;
		rb->tail = -1;
	}
}

/**
 * @brief set max and min values to the ring buffer
 * @param rb
 * @param max
 * @param min
 * @return
 */
bool rb_set_maxmin(rb_t* rb, float max, float min)
{
	if( !rb || !rb->buff )
	{
		log_error("[rb_set_maxmin] Null pointer");
		return false;
	}

	if( isnanf(max) || isnanf(min) )
	{
		log_error("[rb_set_maxmin] NaN");
		return false;
	}

	rb->max = max;
	rb->min = min;
	return true;
}

bool rb_set_headtail(rb_t* rb, int16_t h, int16_t t)
{
	if( !rb || !rb->buff )
	{
		log_error("[rb_set_ht] Null pointer");
		return false;
	}

	if( h<0 || h>=rb->capacity )
	{
		log_warn("[rb_set_ht] invalid head idx");
		h = 0;
	}
	if( t<-1 || h>=rb->capacity )
	{
		log_warn("[rb_set_ht] invalid tail idx");
		t = rb->capacity-1;
	}
	rb->head = h;
	rb->tail = t;
	return true;
}


/**
* @brief: update the maximum value of a full-size ring buffer via traversing the
*		  buffer to search for maximum.
* @params:
*	rb [in]: ring buffer pointer
* @retval: none
*/
static void rb_update_max_traverse(rb_t* rb)
{
	int16_t i;
	float max = rb->buff[0];

	for(i=1; i<rb->capacity; ++i)
	{
		if( rb->buff[i] > max )
			max = rb->buff[i];
	}
	rb->max = max;
}

/**
* @brief: update the minimum value of a full-size ring buffer via traversing the
*		  buffer to search for minimum.
* @params:
*	rb [in]: ring buffer pointer
* @retval: none
*/
static void rb_update_min_traverse(rb_t* rb)
{
	int16_t i;
	float min = rb->buff[0];

	for(i=1; i<rb->capacity; ++i)
	{
		if( rb->buff[i] < min )
			min = rb->buff[i];
	}
	rb->min = min;
}

/**
* @brief: update the maximum value of a full-size ring buffer more efficiently
*		  via checking the newly pushed in element, and then judge if needed to
*		  traverse the whole buffer to search for the new maximum.
* @params:
*	rb [in]: ring buffer pointer
*	v_in [in]: newly pushed in element value
*	v_out [in]: lastest popped out elment value
* @retval: none
*/
static void rb_update_max_compare(rb_t* rb, float v_in, float v_out)
{
	// if newly pushed in element is larger than array's maximum, O(1) operation
	if( v_in > rb->max )
		rb->max = v_in;
	// popped out element is equal to the array's maximum, then traversing
	else if (isequalf(v_out, rb->max))
	{
		rb_update_max_traverse(rb);
	}
}

/**
* @brief: update the minimum value of a full-size ring buffer more efficiently
*		  via checking the newly pushed in element, and then judge if needed to
*		  traverse the whole buffer to search for the new maximum.
* @params:
*	rb [in]: ring buffer pointer
*	v_in [in]: newly pushed in element value
*	v_out [in]: lastest popped out elment value
* @retval: none
*/
static void rb_update_min_compare(rb_t* rb, float v_in, float v_out)
{
	if( v_in < rb->min )
		rb->min = v_in;
	else if (isequalf(v_out, rb->min))
	{
		rb_update_min_traverse(rb);
	}
}

/**
 * @brief comparing two given floating numbers to check if they are 'equal'.
 *        comparing floating numbers with '==' is not a good idea!!!
 * @param a[in]
 * @param b[in]
 * @return true if 'a' and 'b' are 'equal'
 */
static bool isequalf(float a, float b)
{
	// skip if they're equal, handles infinity
	if (a == b) return true;
	// check if values are NaN
	if (isnanf(a) || isnanf(b))
	{
		log_warn("[rb] a or b NaN");
		return false;
	}
	// check if values are infinity
	if (isinff(a) || isinff(b))
	{
		log_warn("[rb] a or b Inf");
		return false;
	}
	// scale epsilon proportionally to inputs
	float diff = fabsf(a-b);
	float absa = fabsf(a);
	float absb = fabsf(b);
	if( a==0 || b==0 || diff<MIN_NORMAL )
	{// a or b is 0 or both are extremely close to 0
		if( diff < (FLT_EPSILON * MIN_NORMAL) )
		{
			log_debug("[rb comp] a:%.6f, b:%.6f", a, b);
			return true;
		}
		else
			return false;
	}
	else
	{// use relative error
		if( (diff/MIN(absa+absb, FLT_MAX)) < FLT_EPSILON )
		{
			log_debug("[rb comp] a:%.6f, b:%.6f", a,b);
			return true;
		}
		else
			return false;
	}
//	return diff <= EPSILON;
//	return true;
}

// ******************************************
// Circular Buffer for type int16
// ******************************************
/**
 * @brief initialize an int16_t type circular buffer
 * @param rb
 * @param buff
 * @param capacity
 * @param init_v
 * @return
 */
bool rb_int16_init(rb_int16_t* rb, int16_t* buff, int16_t capacity)
{
	if( !rb || !buff || capacity<=0 )
		return false;

	rb->buff = buff;
	rb->max = -0x7FFF;
	rb->min = 0x7FFF;
	rb->capacity = capacity;
	rb->head = 0;
	rb->size = 0;
	rb->tail = -1;

	return true;
}

/**
 * @brief push a new int16_t element into circular buffer
 * @param rb
 * @param v_in
 * @param v_out
 * @param flag
 * @return
 */
bool rb_int16_push(rb_int16_t* rb, int16_t v_in, int16_t* v_out, bool flag)
{
	if(!rb || !rb->buff)
	{
		log_error("[rb_init16_push] Null pointer");
		return false;
	}

	// full, then pop 1 element and then push
	if (rb->size == rb->capacity)
	{
		*v_out = rb->buff[rb->head];
		// shifting rb->head
		if (++rb->head >= rb->capacity)
			rb->head = 0;
		--rb->size;
	}

	// updating rb->tail
	if (++rb->tail >= rb->capacity)
		rb->tail = 0;
	rb->buff[rb->tail] = v_in;
	++rb->size;

	// If needed to update max/min
	if (flag)
	{
		// update max
		if( rb->max < v_in )
			rb->max = v_in;
		else if( rb->max == *v_out )
		{
			rb->max = -0x7FFF;
			for (int16_t i = 0; i < rb->capacity; ++i)
				if (rb->max < rb->buff[i])
					rb->max = rb->buff[i];
		}
		// update min
		if( rb->min > v_in )
			rb->min = v_in;
		else if( rb->min == *v_out )
		{
		    rb->min = 0x7FFF;
		    for (int16_t i=0; i<rb->capacity; ++i)
		    	if (rb->min > rb->buff[i])
		    		rb->min = rb->buff[i];
		}
	}

	return true;
}

/**
 * @brief pop out the oldest element from circular buffer
 * @param rb
 * @param v_out
 * @return
 */
bool rb_int16_pop(rb_int16_t* rb, int16_t* v_out)
{
	// if empty, no pop operation and return false
	if(!rb || rb->size==0 )
	{
		log_debug("[rb_init16_pop] Null pointer or empty");
		return false;
	}

	*v_out = rb->buff[rb->head];

	// shifting(updating) rb->head
	if (++rb->head >= rb->capacity)
		rb->head = 0;
	--rb->size;

	return true;
}

/**
 *
 * @param rb
 * @param offset
 * @param v_out
 * @return
 */
bool rb_int16_get(rb_int16_t* rb, int16_t offset, int16_t* v_out)
{
	if (!rb || !rb->buff || rb->size == 0)
	{
		log_error("[rb_i16_get] Null pointer or empty");
		return false;
	}
	// invalid offset
	if (-offset >= rb->size || offset >= rb->size)
	{
		log_error("[rb_i16_get] Invalid offset: %d", offset);
		return false;
	}

	int16_t idx = rb->tail + offset;

	if (idx >= rb->capacity)
		idx -= rb->capacity;
	else if (idx < 0)
		idx += rb->capacity;

	*v_out = rb->buff[idx];
	return true;
}

/**
 *
 * @param rb
 * @return
 */
bool rb_int16_isfull(rb_int16_t* rb)
{
	if( !rb )
	{
		log_error("[rb_int16_isfull] Null pointer");
		return false;
	}
	return (rb->size==rb->capacity);
}

/**
 *
 * @param rb
 * @return
 */
bool rb_int16_isempty(rb_int16_t* rb)
{
	if( !rb )
	{
		log_error("[rb_int16_isempty] Null pointer");
		return false;
	}
	return (rb->size==0);
}

/**
 *
 * @param rb
 */
void rb_int16_free(rb_int16_t* rb)
{
	if( !rb )
	{
		log_debug("[rb_int16_free] Null pointer");
		return;
	}
	rb->buff = NULL;
	rb->capacity = 0;
	rb->size = 0;
	rb->max = 0;
	rb->min = 0;
	rb->head = -1;
	rb->tail = -1;
}

/**
 *
 * @param rb
 * @param buff
 * @param capacity
 * @return
 */
bool rb_int32_init(rb_int32_t* rb, int32_t* buff, int16_t capacity)
{
	if (!rb || !buff || capacity <= 0)
	{
		log_error("[rb i32] init err");
		return false;
	}
	rb->buff = buff;
	rb->max = -RB_MAX;
	rb->min = RB_MAX;
	rb->capacity = capacity;
	rb->head = 0;
	rb->size = 0;
	rb->tail = -1;
	return true;
}

/**
 *
 * @param rb
 * @param v_in
 * @param v_out
 * @param flag
 * @return
 */
bool rb_int32_push(rb_int32_t* rb, int32_t v_in, int32_t* v_out, bool flag)
{
	if( !rb || !rb->buff )
	{
		log_error("[rb i32 push] null pointer");
		return false;
	}
	//if full then pop an element
	if( rb->size == rb->capacity )
	{
		*v_out = rb->buff[rb->head];
		if( ++rb->head >= rb->capacity ) rb->head = 0;
		--rb->size;
	}
	//shift tail idx if needed
	if( ++rb->tail >= rb->capacity ) rb->tail = 0;
	rb->buff[rb->tail] = v_in;
	++rb->size;
	// update max/min if needed
	if( flag )
	{
		//update max
		if( rb->max < v_in )
			rb->max = v_in;
		else if( rb->max == *v_out )
		{
			rb->max = -RB_MAX;
			for( int16_t i = 0; i < rb->capacity; ++i )
				if( rb->max < rb->buff[i] )
					rb->max = rb->buff[i];
		}
		//update min
		if( rb->min > v_in )
			rb->min = v_in;
		else if( rb->min == *v_out )
		{
			rb->min = RB_MAX;
			for( int16_t i = 0; i < rb->capacity; ++i )
				if( rb->min > rb->buff[i] )
					rb->min = rb->buff[i];
		}
	}
	return true;
}

bool rb_int32_pop(rb_int32_t* rb, int32_t* v_out)
{
	if( !rb || rb->size==0 )
	{
		log_error("[rb i32 pop] null pointer or empty");
		return false;
	}
	*v_out = rb->buff[rb->head];
	//shift index if needed
	if( ++rb->head >= rb->capacity ) rb->head = 0;
	--rb->size;
	return true;
}

bool rb_int32_get(rb_int32_t* rb, int16_t offset, int32_t* v_out)
{
	if (!rb || !rb->buff || rb->size == 0)
	{
		log_error("[rb_i32_get] Null pointer or empty");
		return false;
	}

	if (-offset >= rb->size || offset >= rb->size)// invalid offset
	{
		log_error("[rb_i32_get] Invalid offset: %d", offset);
		return false;
	}

	int16_t idx = rb->tail + offset;

	if (idx >= rb->capacity)
		idx -= rb->capacity;
	else if (idx < 0)
		idx += rb->capacity;

	*v_out = rb->buff[idx];
	return true;
}

bool rb_int32_isfull(rb_int32_t* rb)
{
	if( !rb )
	{
		log_error("[rb i32 isfull] null pointer");
		return false;
	}
	return (rb->size==rb->capacity);
}

bool rb_int32_isempty(rb_int32_t* rb)
{
	if( !rb )
	{
		log_error("[rb i32 isempty] null pointer");
		return false;
	}
	return (rb->size==0);
}

void rb_int32_free(rb_int32_t* rb)
{
	if( !rb ) return;
	rb->buff = NULL;
	rb->size = 0;
	rb->max = 0;
	rb->min = 0;
	rb->head = -1;
	rb->tail = -1;
}
#pragma GCC pop_options
