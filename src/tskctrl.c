/*
 * tskctrl.c
 *
 *  Created on: Oct 25, 2016
 *      Author: Myant
 */


#include "tskctrl.h"
#include "FreeRTOSConfig.h"
#include "semphr.h"
#include <string.h>

#include "diagnostic.h"
#include "wdt.h"
#include "hal_gpio.h"

/* HEAP_SIZE is the length of heap in bytes. This constant does not define
 * the heap size and is used only for inspection purposes. The heap is defined
 * in startup file and this constant should reflect the real heap size!*/
#define HEAP_SIZE       17408
/* configMAX_PRIORITIES is defined in FreeRTOSConfig.h and is used here to
 * check  if all the priorities are valid */
#define MAX_PRIORITY    configMAX_PRIORITIES

#define INVALID_POSITION    -1

#define STACK(REAL,SAFETY_LEVEL) ((uint16_t)(REAL*SAFETY_LEVEL))

#define OPMODE_STACK    320
static const TaskData task_list[] =
{
    //-------------------- highest priority ------------------
    {TSK_GTK_SPI,           "gtkspi",   STACK(250,2.0),      5},
    //--------------------------------------------------------
    {TSK_LIB_BLE_EVENTS,    "ble_evt",  STACK(260,2.0),     4},
    {TSK_LIB_IOLOG,         "iolog",    STACK(50,1.3),      4},
    //--------------------------------------------------------
    {TSK_HEATER_HARDWARE,   "heathw",   STACK(200,1.5),     3},
	{TSK_HEATER_CTRL,		"heatctl",	STACK(200,1.5),		3},
    //--------------------------------------------------------
    {TSK_BLE_PUSH,          "blepush",  STACK(149,1.5),     2},
	{TSK_SYSTEM,            "system",   STACK(170,1.3),     2},
    {TSK_LIB_CLI,           "cli",      STACK(260,1.5),     2},
	{TSK_CLI_HEAT,			"cliheat",	STACK(260,1.3),		2},
	{TSK_PMIC_WDT,          "pmicwdt",  STACK(150,1.3),     2},
	{TSK_SENS_TEMP,         "sens_temp",STACK(149,1.5),     2},
    //--------------------- lowest priority ------------------
    /* TSK_DIAGNOSTICS will start as highest priority task just to set the
    internal time stamp. Once time is set, priority will be set back to 1 */
    {TSK_DIAGNOSTICS,       "diagn",    STACK(200,1.3),     TSK_DIAG_PRIO_MAX},
    {TSK_WDT,               "wdt",      STACK(200,1.3),     1},
};
static const uint16_t NTASKS = sizeof(task_list)/sizeof(task_list[0]);
static TaskHandle_t task_handles[MAX_TASKS] = {0};
static uint8_t stack_usage[MAX_TASKS] = {0};
static EventGroupHandle_t evt_handle = NULL;
static EventBits_t last_events = 0;
static bool stack_warning = false;
static bool stack_critical = false;
//tracks the usage of the heap by tasks stacks (in bytes)
static uint32_t used_heap_tasks = 0;
//tracks the usage of the heap by queues (in bytes)
static uint32_t used_heap_queues = 0;

static uint8_t past_stack_usage[MAX_TASKS] __attribute__ ((section (".noinit")));
static bool preserve_stack = false;

static int16_t get_tsk_index(TaskId id);

#if configSUPPORT_STATIC_ALLOCATION == 1
#define IDLE_TASK_SIZE 80
/* static memory allocation for the IDLE task */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[IDLE_TASK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
        StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = IDLE_TASK_SIZE;
}
#endif

#if configSUPPORT_STATIC_ALLOCATION && configUSE_TIMERS
static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

/* If static allocation is supported then the application must provide the
   following callback function - which enables the application to optionally
   provide the memory that will be used by the timer task as the task's stack
   and TCB. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
        StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif

bool os_init(void)
{
    if(NTASKS != MAX_TASKS)
    {
        /*
         * Tasks declaration and configuration wrong! Check task_list and
         * TaskId. Structures must have the same number of elements!
         */
        return false;
    }
    //check if all priorities are correct
    uint16_t i;
    const TaskData *tsk = task_list;
    for(i = 0; i < NTASKS; i++, tsk++)
    {
        if(tsk->priority >= MAX_PRIORITY)
            return false;
    }
    evt_handle = xEventGroupCreate();
    ASSERT_EVTGRP(evt_handle);
    last_events = 0;
	//check if last reset stack should be preserved
    preserve_stack = diag_is_rst(RSTSRC_HARDF) || diag_is_rst(RSTSRC_WDT);
    if(!preserve_stack)
        memset(past_stack_usage, 0, sizeof(past_stack_usage));
    return true;
}

TaskHandle_t tsk_create(TaskId id, tsk_entry_t ep)
{
    if(NTASKS != MAX_TASKS)
        return NULL;
    if(NULL == ep)
        return NULL;
    int16_t ix = get_tsk_index(id);
    if(INVALID_POSITION == ix)
        return NULL;
    //task already created
    if(task_handles[ix] != NULL)
        return task_handles[ix];
    //create task
    TaskHandle_t hnd = NULL;
    BaseType_t status;
    const TaskData *t = &task_list[ix];

    status = xTaskCreate(ep, t->name, t->stack, NULL, t->priority, &hnd);
    if(pdPASS == status)
    	used_heap_tasks += (t->stack*4);

    if(pdPASS == status)
    {
        task_handles[ix] = hnd;
        return hnd;
    }
    return NULL;
}

TaskHandle_t tsk_get_handle(TaskId id)
{
    if(NTASKS != MAX_TASKS)
        return NULL;
    int16_t ix = get_tsk_index(id);
    if(INVALID_POSITION == ix)
        return NULL;
    return task_handles[ix];
}

void os_evt_trigger(evt_app_id id)
{
    if(evt_handle)
    {   
        wdt_trigger_event(id);
        if(is_irq_context())
        {
            BaseType_t context_switch = pdFALSE;
            xEventGroupSetBitsFromISR(evt_handle, id, &context_switch);
            portYIELD_FROM_ISR(context_switch);
        }
        else
            xEventGroupSetBits(evt_handle, id);
    }
}

EventBits_t os_evt_wait(EventBits_t events, TickType_t timeout)
{
    EventBits_t new_events;
    if(evt_handle)
    {
        xEventGroupWaitBits(evt_handle, events, pdFALSE, pdFALSE, timeout);
        new_events = xEventGroupGetBits(evt_handle);
        xEventGroupClearBits(evt_handle, events);
        last_events |= new_events;
    }
    return last_events;
}

void os_evt_clear(EventBits_t events)
{
    if(evt_handle)
        last_events &= ~events;
}

EventBits_t os_evt_get(void)
{
    EventBits_t evt = 0;
    if(evt_handle)
        evt = last_events;
    return evt;
}

static int16_t get_tsk_index(TaskId id)
{
    uint16_t i;
    int16_t ix = INVALID_POSITION;
    const TaskData *tsk = task_list;

    for(i = 0; i < NTASKS; i++, tsk++)
    {
        if(tsk->id == id)
        {
            ix = i;
            break;
        }
    }
    return ix;
}

void os_delay_ms(uint32_t time_ms)
{
	vTaskDelay(pdMS_TO_TICKS(time_ms));
}

void os_delay_until_ms(uint32_t time_ms, void *previous_tick)
{
	TickType_t *previous = (TickType_t*)previous_tick;
	vTaskDelayUntil(previous, pdMS_TO_TICKS(time_ms));
}

app_status_t os_create_mutex(void *mutex_handler)
{
	SemaphoreHandle_t *handler = (SemaphoreHandle_t*)mutex_handler;
	*handler = xSemaphoreCreateMutex();
	if(NULL == *handler)
		return APPST_OS_ERROR;
	return APPST_SUCCESS;
}

bool os_get_mutex(void *mutex_handler, uint32_t timeout_ms)
{
	BaseType_t ret = pdFAIL;
	BaseType_t context_switch = pdFALSE;
	SemaphoreHandle_t *handler = (SemaphoreHandle_t*)mutex_handler;
	if(os_is_scheduler_on())
	{
		if(*handler)
		{
			if(is_irq_context())
			{
				ret = xSemaphoreTakeFromISR(*handler, &context_switch);
				portYIELD_FROM_ISR(context_switch);
			}
			else
			{
				ret = xSemaphoreTake(*handler, pdMS_TO_TICKS(timeout_ms));
			}
		}
		return (pdPASS == ret);
	}
	return true;
}

void os_release_mutx(void *mutex_handler)
{
	SemaphoreHandle_t *handler = (SemaphoreHandle_t*)mutex_handler;
    if(os_is_scheduler_on() && *handler)
    {
        TaskHandle_t owner, actual;
        owner = xSemaphoreGetMutexHolder(*handler);
        actual = self_task();
        if(actual == owner)
            xSemaphoreGive(*handler);
    }
}

inline bool os_is_scheduler_on(void)
{
    BaseType_t state = xTaskGetSchedulerState();
    return (taskSCHEDULER_RUNNING == state);
}

bool os_is_stack_warning(void)
{
    return stack_warning;
}

bool os_is_stack_critical(void)
{
    return stack_critical;
}

//calculate % of used stack
void os_update_stack_level(void)
{
    uint8_t i;
    BaseType_t water_mark;
    const TaskData *tsk = task_list;
    uint8_t *usage = stack_usage;
    float percent;

    for(i = 0; i < NTASKS; i++, tsk++, usage++)
    {
        if(task_handles[i] && tsk->stack)
        {
            water_mark = uxTaskGetStackHighWaterMark(task_handles[i]);
            percent = ((float)(tsk->stack-water_mark))/tsk->stack*100.0;
            *usage = (uint8_t)(percent+0.5); //+0.5 to round up decimal
            if(*usage > STACK_LEVEL_WARNING)
                stack_warning = true;
            if(*usage > STACK_LEVEL_CRITICAL)
                stack_critical = true;
        }
        else
            *usage = 0;
    }
	if(!preserve_stack)
        memcpy(past_stack_usage, stack_usage, sizeof(stack_usage));
}

const TaskData  *os_get_taskconfig(TaskId id)
{
    if(NTASKS != MAX_TASKS)
        return NULL;
    int16_t ix = get_tsk_index(id);
    if(INVALID_POSITION == ix)
        return NULL;
    return &task_list[ix];
}

bool os_get_stack_usage(uint8_t *buffer, uint8_t len, bool lastrst)
{
    if(len < NTASKS)
        return false;
    if(lastrst)
    {
        memcpy(buffer, past_stack_usage, sizeof(past_stack_usage));
        preserve_stack = false;
    }
    else
        memcpy(buffer, stack_usage, sizeof(stack_usage));
    return true;
}

QueueHandle_t os_create_queue(UBaseType_t length, UBaseType_t itemsize)
{
    QueueHandle_t handle = xQueueCreate(length, itemsize);
    if(NULL != handle)
        used_heap_queues += (length*itemsize);
    return handle;
}

uint32_t os_heap_size(void)
{
    return HEAP_SIZE;
}

uint32_t os_heap_tsk_usage(void)
{
    return used_heap_tasks;
}

uint32_t os_heap_queue_usage(void)
{
    return used_heap_queues;
}
