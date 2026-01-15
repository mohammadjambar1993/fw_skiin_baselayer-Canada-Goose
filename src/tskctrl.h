/*
 * tskctrl.h
 *
 *  Created on: Oct 25, 2016
 *      Author: Myant
 */

#ifndef SRC_TSKCTRL_H_
#define SRC_TSKCTRL_H_

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "apptypes.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#define STACK_LEVEL_WARNING     75
#define STACK_LEVEL_CRITICAL    95

#define TSK_DIAG_PRIO_STD   1
#define TSK_DIAG_PRIO_MAX   5

//useful macros
#define is_irq_context()(0 != __get_IPSR())
#define self_task()     xTaskGetCurrentTaskHandle()
#define ticks_to_ms(x)  ((uint32_t)(((portTickType)x)*portTICK_PERIOD_MS))
#define WAIT_FOREVER    portMAX_DELAY
#define DONT_WAIT       0
#if ENABLE_TEST_FUNCTIONS == 1
    #define ASSERT_TASK(x)  {while(x == NULL){__NOP();}}
    #define ASSERT_QUEUE(x) {while(x == NULL){__NOP();}}
    #define ASSERT_MUTEX(x) {while(x == NULL){__NOP();}}
    #define ASSERT_TIMER(x) {while(x == NULL){__NOP();}}
    #define ASSERT_EVTGRP(x){while(x == NULL){__NOP();}}
    #warning "ASSERTS are in debug mode!"
#else
#define ASSERT_TASK(x)  {while(x == NULL){__NOP();}}
    #define ASSERT_QUEUE(x) {while(x == NULL){NVIC_SystemReset();}}
    #define ASSERT_MUTEX(x) {while(x == NULL){NVIC_SystemReset();}}
    #define ASSERT_TIMER(x) {while(x == NULL){NVIC_SystemReset();}}
    #define ASSERT_EVTGRP(x){while(x == NULL){NVIC_SystemReset();}}
#endif

typedef enum
{
    TSK_GTK_SPI,
    TSK_LIB_BLE_EVENTS,
    TSK_LIB_IOLOG,
	TSK_HEATER_HARDWARE,
	TSK_HEATER_CTRL,
    TSK_BLE_PUSH,
	TSK_SYSTEM,
    TSK_LIB_CLI,
	TSK_CLI_HEAT,
	TSK_PMIC_WDT,
    TSK_DIAGNOSTICS,
    TSK_WDT,
	TSK_SENS_TEMP,
    //do not define new tasks after MAX_TASKS!
    MAX_TASKS
}TaskId;

typedef enum
{
    EVT_BLE_CONNECTED    	 = (1<<0),
    EVT_BLE_DISCONNECTED 	 = (1<<1),
    EVT_BLE_CONN_UPDATE  	 = (1<<2),
    EVT_BLE_CHAR_WRITE   	 = (1<<3),
    EVT_BLE_CCCD_WRITE   	 = (1<<4),
    EVT_BLE_COMMAND      	 = (1<<5),
	EVT_HEATING_OFF      	 = (1<<6),
	EVT_HEAT_SAMPLE	     	 = (1<<7),
	EVT_HEAT_SHORT		 	 = (1<<8),
	EVT_HARDWARE_FAIL	 	 = (1<<9),
	EVT_BLE_HEAT_INFO	 	 = (1<<10),
	EVT_HEAT_NEW_SESSION 	 = (1<<11),
	EVT_HEAT_SESSION_TIMEOUT = (1<<12),
    EVT_MOD_UPDATE_INFO	     = (1<<13),
}evt_app_id;

typedef struct
{
    TaskId id;
    const char *name;
    uint16_t stack;
    UBaseType_t priority;
}TaskData;

typedef void(*tsk_entry_t)(void *params);

bool os_init(void);
TaskHandle_t tsk_create(TaskId id, tsk_entry_t ep);
TaskHandle_t tsk_get_handle(TaskId id);
bool os_is_scheduler_on(void);
void os_evt_trigger(evt_app_id id);
EventBits_t os_evt_wait(EventBits_t events, TickType_t timeout);
void os_evt_clear(EventBits_t events);
EventBits_t os_evt_get(void);
void os_delay_ms(uint32_t time_ms);
void os_delay_until_ms(uint32_t time_ms, void *previous_tick);
void os_update_stack_level(void);
bool os_get_stack_usage(uint8_t *buffer, uint8_t len, bool lastrst);
bool os_is_stack_warning(void);
bool os_is_stack_critical(void);
const TaskData *os_get_taskconfig(TaskId id);
QueueHandle_t os_create_queue(UBaseType_t length, UBaseType_t itemsize);
uint32_t os_heap_size(void);
uint32_t os_heap_tsk_usage(void);
uint32_t os_heap_queue_usage(void);
app_status_t os_create_mutex(void *mutex_handler);
bool os_get_mutex(void *mutex_handler, uint32_t timeout_ms);
void os_release_mutx(void *mutex_handler);

static inline void tsk_suspend_by_id(TaskId id)
{
    TaskHandle_t hnd = tsk_get_handle(id);
    if(NULL != hnd)
        vTaskSuspend(hnd);
}

static inline void tsk_suspend_by_hnd(TaskHandle_t hnd)
{
    if(NULL != hnd)
        vTaskSuspend(hnd);
}

static inline void tsk_resume_by_id(TaskId id)
{
    TaskHandle_t hnd = tsk_get_handle(id);
    if(NULL != hnd)
    {
        if(is_irq_context())
            xTaskResumeFromISR(hnd);
        else
            vTaskResume(hnd);
    }
}

static inline void tsk_resume_by_hnd(TaskHandle_t hnd)
{
    if(NULL != hnd)
    {
        if(is_irq_context())
            xTaskResumeFromISR(hnd);
        else
            vTaskResume(hnd);
    }
}

static inline eTaskState tsk_state_by_id(TaskId id)
{
    eTaskState etsk;
    TaskHandle_t hnd = tsk_get_handle(id);
    if(NULL == hnd)
        etsk = eInvalid;
    else
        etsk = eTaskGetState(hnd);
    return etsk;
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_TSKCTRL_H_ */
