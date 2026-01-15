/*
 * maf.c
 *
 *  Created on: Jan 10, 2018
 *      Author: Dingchen (David)
 */
#include "maf.h"

/**
 * @brief initialize the moving average filter
 * @param maf [in]: moving average filter to be initialized
 * @param array [in]: pre-defined fixed length buffer
 * @param size [in]: length of the moving average filter
 * @param init_val [in]: initial value of buffer
 * @return true if initialization is successful, otherwise false
 */
bool maf_init(maf_t* maf, int16_t* array, int16_t size, int16_t init_val)
{
	if( !maf || !array || size==0 )
	{
		return false;
	}
	// initialization
	maf->buff = array;
	maf->avg = init_val;
	maf->size = size;
	maf->head = 0;
	maf->tail = size-1;
	for(int16_t i=0; i<size; ++i)
		maf->buff[i] = init_val;

	return true;
}

/**
 * @brief push in a new sample into the moving average filter and updates the
 * 		  average value
 * @param maf [in]: moving average filter to be operated
 * @param v_in [in]: pushed in data
 * @retval none
 */
void maf_pushback(maf_t* maf, int16_t v_in)
{
	// Assume 'maf_t' has been successfully initialized and
	// Also the buffer is always full before pushing in new data
	int16_t v_out = maf->buff[maf->head];
	// shifting head index
	if (++maf->head >= maf->size)
		maf->head = 0;
	// updating tail index and push in data
	if (++maf->tail >= maf->size)
		maf->tail = 0;
	maf->buff[maf->tail] = v_in;
	// updating average value
	maf->avg += ((float)(v_in-v_out))/maf->size;
}

/**
 *
 * @param maf [in]: moving average filter
 * @return the updated average value of moving average filter
 */
float maf_get_avg(maf_t* maf)
{
	return maf->avg;
}


/*
 * Operations for new int16 moving average filter which is capable of updating
 * maximum and minimum value within the buffer
 */
/**
 * @brief moving average filter initialization, each element is int16_t
 * @param maf [in]: moving average filter to be initialized
 * @param buff [in]: pre-defined fixed length buffer
 * @param capacity [in]: length of the moving average filter
 * @param init_v [in]: initial value of buffer
 * @return true if successfully initialized, otherwise false
 */
bool maf_int16_init(maf_int16_t* maf, int16_t* buff, int16_t size, int16_t init_v)
{
	if (!maf || !buff || size==0)
	{
		return false;
	}
	// initialization
	maf->buff = buff;
	maf->avg = init_v;
	maf->max = -0x7FFF;
	maf->min = 0x7FFF;
	maf->size = size;
	maf->head = 0;
	maf->tail = size - 1;
	for (int16_t i = 0; i < size; ++i)
		maf->buff[i] = init_v;

	return true;
}

/**
 * @brief push in a new sample into the moving average filter and updates the
 * 	      average, maximum, and minimum of the buffer
 * @param maf [in]: moving average filter buffer pointer
 * @param v_in [in]: pushed in data
 */
void maf_int16_push(maf_int16_t* maf, int16_t v_in)
{
	// Assume 'maf_t' has been successfully initialized and
	// Also the buffer is always full before pushing in new data
	int16_t v_out = maf->buff[maf->head];
	// shifting head index
	if (++maf->head >= maf->size)
		maf->head = 0;
	// updating tail index and push in data
	if (++maf->tail >= maf->size)
		maf->tail = 0;
	maf->buff[maf->tail] = v_in;
	// updating average value
	maf->avg += ((float) (v_in - v_out)) / maf->size;
	// updating maximum
	if (maf->max < v_in)
		maf->max = v_in;
	else if (maf->max == v_out)
	{
		// traverse to search for new max
		maf->max = -0x7FFF;
		for(int16_t i=0; i<maf->size; ++i)
		{
			if (maf->max < maf->buff[i])
				maf->max = maf->buff[i];
		}
	}
	// updating minimum
	if (maf->min > v_in)
		maf->min = v_in;
	else if (maf->min == v_out)
	{
		maf->min = 0x7FFF;
		for(int16_t i=0; i<maf->size; ++i)
		{
			if (maf->min > maf->buff[i])
				maf->min = maf->buff[i];
		}
	}
}

/**
 * @brief returns the moving average
 * @param maf [in]: moving average filter buffer pointer
 * @return the moving average
 */
float maf_int16_get_avg(maf_int16_t* maf)
{
	return maf->avg;
}

/**
 * @brief returns the maximum of the moving average filter buffer
 * @param maf [in]: moving average filter buffer pointer
 * @return the maximum of filter buffer
 */
int16_t maf_int16_get_max(maf_int16_t* maf)
{
	return maf->max;
}

/**
 * @brief returns the minimum of the moving average filter buffer
 * @param maf [in]: moving average filter buffer pointer
 * @return the minimum of filter buffer
 */
int16_t maf_int16_get_min(maf_int16_t* maf)
{
	return maf->min;
}
