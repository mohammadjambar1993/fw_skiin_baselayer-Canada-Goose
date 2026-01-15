/*
 * wdt.c
 *
 *  Created on: Jun 21, 2018
 *      Author: Myant
 */

#include "wdt.h"

#include "diagnostic.h"
#include "semphr.h"
#include "logger.h"
#include "nrf_drv_wdt.h"

//-1 because the watchdog task doesnt need to be monitored
#define N_TASKS         (MAX_TASKS-1)
#define TIME_WDT_TASK   500 //in ms

typedef struct
{
    TaskId id;
    bool enabled;
    wdt_type_t type;
    uint32_t timeout;
    uint32_t counter;
    evt_app_id event_mask;
    bool event_set;
}wdt_check_t;

static uint16_t tsk_entry_ct = 0;
static SemaphoreHandle_t mutex = NULL;
static wdt_check_t tsk_entries[N_TASKS] = {{0}};
static nrf_drv_wdt_channel_id wdt_channel;
#if ENABLE_TEST_FUNCTIONS == 1
static bool ignore_feed = false;
#endif

static void tsk_wdt(void *params);
static void wdt_irq(void);

static inline bool get_mtx(TickType_t wait)
{
    BaseType_t ret = pdFAIL;
    BaseType_t context_switch = pdFALSE;
    if(os_is_scheduler_on() && mutex)
    {
        if(is_irq_context())
        {
            ret = xSemaphoreTakeFromISR(mutex, &context_switch);
            portYIELD_FROM_ISR(context_switch);
        }
        else
        {
            ret = xSemaphoreTake(mutex, wait);
        }
        return (pdPASS == ret);
    }
    return true;
}

static inline void release_mtx(void)
{
    if(os_is_scheduler_on() && mutex)
    {
        BaseType_t context_switch = pdFALSE;
        if(is_irq_context())
        {
            xSemaphoreGiveFromISR(mutex, &context_switch);
            portYIELD_FROM_ISR(context_switch);
        }
        else
        {
            TaskHandle_t owner, actual;
            owner = xSemaphoreGetMutexHolder(mutex);
            actual = self_task();
            if(actual == owner)
                xSemaphoreGive(mutex);
        }
    }
}

void wdt_init(void)
{
    if(NULL == mutex)
    {
        ASSERT_TASK(tsk_create(TSK_WDT, tsk_wdt));
        mutex = xSemaphoreCreateMutex();
        ASSERT_MUTEX(mutex);
    }
}

void wdt_start(void)
{
    nrf_drv_wdt_config_t config = NRF_DRV_WDT_DEAFULT_CONFIG;
    uint32_t err_code = nrf_drv_wdt_init(&config, wdt_irq);
    if(NRF_SUCCESS != err_code)
    {
    	diag_inc_flag(FLG_WDT_INIT);
    	log_error("[wdt] init watchdog: %d", err_code);
        return;
    }
    err_code = nrf_drv_wdt_channel_alloc(&wdt_channel);
    if(NRF_SUCCESS != err_code)
    {
    	diag_inc_flag(FLG_WDT_INIT);
        log_error("[wdt] allocate wdt channel: %d", err_code);
        return;
    }
    nrf_drv_wdt_enable();
}

static void tsk_wdt(void *params)
{
    uint8_t i;
    wdt_check_t *entry;
    bool must_feed;
    TickType_t tick;

    while(1)
    {
        /* if stack of any task is critical, suspend the wdt task and wait
         * the watchdog timer to reset the system. Don't reset right away to
         * give other tasks time to send diagnostics or do some other checks */
        while(os_is_stack_critical())
            vTaskSuspend(self_task());
        entry = tsk_entries;
        must_feed = true;
        tick = xTaskGetTickCount();
        for(i = 0; i < tsk_entry_ct; i++, entry++)
        {
            if((WDT_E_INVALID == entry->type) || !entry->enabled)
                continue;
            if((WDT_E_PERIODIC == entry->type) || (entry->event_set))
            {
                entry->counter++;
                if(entry->counter >= entry->timeout)
                {
                    must_feed = false;
                    //set system flag to identify which task timed out
                    //<<hack>> implement this function
                    diag_flag_wdt_timeout(entry->id);
                    log_debug("[wdt] task %d timed out", entry->id);
                }
            }
        }
        if(must_feed)
        {
            //log_debug("[wdt] feeding watchdog...");
            nrf_drv_wdt_channel_feed(wdt_channel);
        }
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(TIME_WDT_TASK));
    }
}

static bool is_valid_config(wdt_config_t *config)
{
    if(NULL == config)
        return false;
    if(config->type == WDT_E_INVALID)
        return false;
    if((config->id >= MAX_TASKS) && (config->type != WDT_E_IRQ))
        return false;
    return true;
}

wdt_entry wdt_config_entry(wdt_config_t *config, TickType_t wait_ms)
{
    wdt_check_t *entry = NULL;
    if(!is_valid_config(config))
    {
    	diag_inc_flag(FLG_WDT_INIT);
    	log_error("[wdt] invalid configuration");
        return NULL;
    }
    if(tsk_entry_ct == MAX_TASKS)
    {
    	diag_inc_flag(FLG_WDT_INIT);
    	log_error("[wdt] no space for a new entry!");
        return NULL;
    }
    if(get_mtx(wait_ms))
    {
        entry = &tsk_entries[tsk_entry_ct];
        tsk_entry_ct++;
        release_mtx();
        entry->counter = 0;
        entry->enabled = config->enabled;
        entry->event_mask = config->event_mask;
        entry->event_set = false;
        entry->id = config->id;
        entry->timeout = config->timeout;
        entry->type = config->type;
    }
    else
	{
		diag_inc_flag(FLG_WDT_CONFIG);
		log_error("[wdt] initializing entry for id %d", config->id);
	}
    return entry;
}

void wdt_trigger_event(evt_app_id evt)
{
    uint32_t i = 0;
    wdt_check_t *ent = tsk_entries;
    for(i = 0; i < tsk_entry_ct; i++, ent++)
    {
        if(ent->event_mask & evt)
            ent->event_set = true;
    }
}

void wdt_unblock_task(wdt_entry entry)
{
    if(NULL == entry)
        return;
    wdt_check_t *dev = (wdt_check_t*)entry;
    dev->event_set = true;
}

void wdt_ignore_feed(void)
{
#if ENABLE_TEST_FUNCTIONS == 1
    ignore_feed = true;
#endif
}

void wdt_feed(wdt_entry entry)
{
#if ENABLE_TEST_FUNCTIONS == 1
    if(NULL == entry || ignore_feed)
        return;
#else
    if(NULL == entry)
           return;
#endif
    wdt_check_t *dev = (wdt_check_t*)entry;
    dev->counter = 0;
    dev->event_set = false;
}

void wdt_enable(wdt_entry entry, bool en)
{
    if(NULL == entry)
        return;
    wdt_check_t *dev = (wdt_check_t*)entry;
    dev->counter = 0;
    dev->event_set = false;
    dev->enabled = en;
}

void wdt_set_timeout(wdt_entry entry, uint32_t timeout)
{
    if(NULL == entry)
        return;
    wdt_check_t *dev = (wdt_check_t*)entry;
    dev->counter = 0;
    dev->timeout = timeout;
    dev->event_set = false;
}

void wdt_set_event_mask(wdt_entry entry, evt_app_id mask)
{
    if(NULL == entry)
        return;
    wdt_check_t *dev = (wdt_check_t*)entry;
    dev->counter = 0;
    dev->event_set = false;
    dev->event_mask = mask;
}

void wdt_set_type(wdt_entry entry, wdt_type_t type)
{
    if(NULL == entry)
        return;
    wdt_check_t *dev = (wdt_check_t*)entry;
    dev->counter = 0;
    dev->event_set = false;
    dev->event_mask = 0;
    dev->type = type;
}

static void wdt_irq(void)
{

}
