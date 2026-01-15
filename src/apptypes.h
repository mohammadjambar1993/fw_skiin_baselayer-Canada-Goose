/*
 * appdefs.h
 *
 *  Created on: Jul. 30, 2021
 *      Author: tmg
 */

#ifndef SRC_APPTYPES_H_
#define SRC_APPTYPES_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#ifdef TEST_MOCK_STATIC
	#define STATIC
	#ifdef GCC_CROSS_COMPILER
		#error "TEST_MOCK_STATIC is enabled! Fix it and build again!"
	#endif
#else
	#define STATIC static
#endif

#define INVALID_DUTY_CYCLE	0xFF

typedef enum
{
	APPST_SUCCESS 			= 0,
	APPST_ERROR 			= 1, //generic error
	APPST_OS_ERROR			= 2,
	APPST_INVALID_PARAM 	= 3,
	APPST_INVALID_STATE		= 4,
	APPST_INVALID_LENGTH	= 5
}app_status_t;


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_APPTYPES_H_ */
