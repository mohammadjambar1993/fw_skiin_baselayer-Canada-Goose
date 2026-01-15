/*
 * diagnostic.c
 *
 *  Created on: Oct 31, 2017
 *      Author: Myant
 */

#include <string.h>
//#include "nrf52.h"
#include "nrf52833.h"
#include "tskctrl.h"
#include "ble_rpc.h"
#include "logger.h"
#include "task.h"
#include "semphr.h"
#include "hal_gpio.h"
#include "diagnostic.h"
#include "wdt.h"

#define LOG_MODINFO 0x01
#define LOG_STACK   0x02
#define LOG_ERRCT   0x03
#define LOG_WDT     0x04

//used task stack in %. If usage is greater than this value, send log message
#define MAX_STACK_LEVEL     80
#define WAIT_TX_TIME_MS     20
#define MAX_FLAG_COUNTER    0xFFFF
#define N_WDT_TIMEOUTS      5   //how many wdt timeouts to keep

typedef struct
{
    bool locked;
    uint16_t err_ct;
    uint16_t last_err;
}flag_counter_t;

typedef struct
{
    uint16_t max_err_ct;
}flag_max_t;

//keep track of time when the last log message was transmitted
static TickType_t last_sent = 0;
static SemaphoreHandle_t mtxq = NULL;
static bool has_ct_error = false; //there is at least one counter error
static diagnostic_id dump_diagnostic = 0; //when true, send diagnostic messages
static uint16_t last_rst_reason = 0;
static uint8_t blebuffer[LENCHR_LOGGER] = {0};
//reset_mark may be accessed by other modules
static uint32_t reset_mark __attribute__ ((section (".noinit")));
static flag_counter_t ctblock[MAX_FLAGS] __attribute__ ((section (".noinit")));
static TaskId wdt_timeout[N_WDT_TIMEOUTS] __attribute__ ((section (".noinit")));
static const flag_max_t maxct[MAX_FLAGS] = {{50}};
static wdt_entry wdt = NULL;
static bool send_wdt_timeout = false;


static inline bool get_mtx(void);
static inline void release_mtx(void);
static void tsk_diagnostics(void *params);
static void send_modinfo(void);
static void send_wdt_timeouts(void);
static void send_tasks_stack(diagnostic_id diagcmd, bool resetstack);
static void send_error_counters(diagnostic_id diagcmd);
static bool tx_log_message(uint8_t length);
static inline bool get_err_flag(flag_id id, flag_counter_t **f,
                                const flag_max_t **m);

void diag_init(void)
{
    //read last reset source and reset the register
    uint32_t reg;
    uint16_t reason;
    sd_power_reset_reason_get(&reg);
    reason = (uint16_t)((reg & 0x000F0000)>>12);
    reason += (uint16_t)(reg & 0x0000000F);
    if(HARDFAULT_TAG == reset_mark)
        reason |= RSTSRC_HARDF;
    last_rst_reason = reason;
    //clear reset reason register
    sd_power_reset_reason_clr(0xFFFFFFFF);
    sd_power_reset_reason_get(&reg);
    reset_mark = 0;
    if(last_rst_reason & RSTSRC_WDT)
    {
        send_wdt_timeout = true;
    }
    else
    {
        send_wdt_timeout = false;
        memset(wdt_timeout, 0xFF, sizeof(wdt_timeout));
    }

    /* reset cause must be logged using BLE messages. Lock counters until
         * they can be sent to the phone*/
    if((last_rst_reason & RSTSRC_WDT) ||(last_rst_reason & RSTSRC_LOCKUP)||
       (last_rst_reason & RSTSRC_LPCOMP) ||(last_rst_reason & RSTSRC_SYSOFF)||
       (last_rst_reason & RSTSRC_NFC)||(last_rst_reason & RSTSRC_DBGIF)||
	   (last_rst_reason & RSTSRC_HARDF))
    {
    	uint16_t i;
		flag_counter_t *ct = ctblock;
		for(i = 0; i < MAX_FLAGS; i++, ct++)
			ct->locked = true;

    }
    /* if reset was caused by reset pin or power up or soft reset,there is no need to check
        * persistent memory for reset causes */
    else
    {
    	memset(ctblock, 0, sizeof(ctblock));
    }
    if(NULL == tsk_get_handle(TSK_DIAGNOSTICS))
    {
        ASSERT_TASK(tsk_create(TSK_DIAGNOSTICS, tsk_diagnostics));
        mtxq = xSemaphoreCreateMutex();
        ASSERT_MUTEX(mtxq);
        //config watchdog timer
        wdt_config_t wconfig;
        wconfig.id = TSK_DIAGNOSTICS;
        wconfig.enabled = true;
        wconfig.event_mask = 0;
        wconfig.timeout = 40;
        wconfig.type = WDT_E_PERIODIC;
        wdt = wdt_config_entry(&wconfig, pdMS_TO_TICKS(200));
    }
    else
    	log_error("Creating Diagnostic task  failed");
}

static inline bool get_mtx(void)
{
    BaseType_t ret = pdFAIL;
    BaseType_t context_switch = pdFALSE;
    if(os_is_scheduler_on())
    {
        if(mtxq)
        {
            if(is_irq_context())
            {
                ret = xSemaphoreTakeFromISR(mtxq, &context_switch);
                portYIELD_FROM_ISR(context_switch);
            }
            else
            {
                ret = xSemaphoreTake(mtxq, DONT_WAIT);
            }
        }
        return (pdPASS == ret);
    }
    return true;
}

static inline void release_mtx(void)
{
    if(os_is_scheduler_on() && mtxq)
    {
        TaskHandle_t owner, actual;
        owner = xSemaphoreGetMutexHolder(mtxq);
        actual = self_task();
        if(actual == owner)
            xSemaphoreGive(mtxq);
    }
}

static inline bool get_err_flag(flag_id id, flag_counter_t **f,
                                const flag_max_t **m)
{
    if(id >= MAX_FLAGS)
        return false;
    if(get_mtx())
    {
        *f = &ctblock[id];
        *m = &maxct[id];
        release_mtx();
        return true;
    }
    return false;
}

uint16_t diag_get_rst_cause(void)
{
    return last_rst_reason;
}

bool diag_is_rst(reset_src src)
{
    return (last_rst_reason & src);
}

void diag_flag_wdt_timeout(TaskId id)
{
    static uint8_t timeout_ct = 0;
    if(send_wdt_timeout) //there are old timeouts to be sent
        return;
    if(N_WDT_TIMEOUTS <= timeout_ct) //timeout array is full
        return;
    if(timeout_ct)//check if past timeout is the same
    {
        if(wdt_timeout[timeout_ct-1] == id)
            return;
    }
    wdt_timeout[timeout_ct] = id;
    timeout_ct++;
}

uint16_t diag_reset_flag(flag_id id)
{
    const flag_max_t *m;
    flag_counter_t *f;

    if(get_err_flag(id, &f, &m))
    {
        if(!f->locked)
            f->err_ct = 0;
        return f->err_ct;
    }
    return MAX_FLAG_COUNTER;
}

uint16_t diag_inc_flag(flag_id id)
{
    const flag_max_t *m;
    flag_counter_t *f;

    if(get_err_flag(id, &f, &m))
    {
        if((!f->locked) && (f->err_ct < MAX_FLAG_COUNTER))
        {
            f->err_ct++;
            has_ct_error = true;
        }
        return f->err_ct;
    }
    return 0;
}

uint16_t diag_dec_flag(flag_id id)
{
    const flag_max_t *m;
    flag_counter_t *f;

    if(get_err_flag(id, &f, &m))
    {
        if((!f->locked) && f->err_ct)
            f->err_ct--;
        return f->err_ct;
    }
    return 0;
}

static bool tx_log_message(uint8_t length)
{
    last_sent = xTaskGetTickCount();
    if(!ble_is_notification_on(BLEMSG_LOGGER))
        return false;

    BLEStatus txst;
    txst = ble_try_notify(BLEMSG_LOGGER, blebuffer, length, 5);

    return (txst == BLEST_OP_OK);
}

#define LEN_STACK_DATA  2
static void send_tasks_stack(diagnostic_id diagcmd, bool resetstack)
{
    uint8_t stack_use[MAX_TASKS], i, ix;
    bool send_all = diagcmd & DIAG_STACK_ALL;

    os_get_stack_usage(stack_use, MAX_TASKS, resetstack);
    memset(blebuffer, 0, sizeof(blebuffer));
    for(i = 0, ix = 1; i < MAX_TASKS; i++)
    {
        if(send_all || (stack_use[i] > MAX_STACK_LEVEL))
        {
            blebuffer[0] = LOG_STACK;
            blebuffer[ix++] = i; //task id
            blebuffer[ix++] = stack_use[i]; //stack level
            /* check if it is possible to put one more message into the ble
             * buffer. If it is not, send the log message */
            if((ix+LEN_STACK_DATA) > (LENCHR_LOGGER-1))
            {
                tx_log_message(ix);
                ix = 1;
                blebuffer[0] = 0;
            }
        }
    }
    if(blebuffer[0] != 0) //buffer has messages that were not sent
    {
        tx_log_message(ix);
        blebuffer[0] = 0;
    }
}

#define LEN_ERRCT_DATA  3
static void send_error_counters(diagnostic_id diagcmd)
{
    bool send_all;
    uint8_t ix;
    uint16_t i;
    flag_counter_t *ct = ctblock;
    const flag_max_t *max = maxct;

    send_all = diagcmd & DIAG_ERRORS_ALL;
    for(i = 0, ix = 1; i < MAX_FLAGS; i++, ct++, max++)
    {
        if(send_all||ct->locked || (ct->err_ct != ct->last_err))
        {
            ct->locked = false;
            ct->last_err = ct->err_ct;
            blebuffer[0] = LOG_ERRCT;
            blebuffer[ix++] = i; //flag id
            memcpy(&blebuffer[ix], &ct->err_ct, sizeof(ct->err_ct));
            ix += sizeof(ct->err_ct);
            /* check if it is possible to put one more message into the ble
             * buffer. If it is not, send the log message */
            if((ix+LEN_ERRCT_DATA) > (LENCHR_LOGGER-1))
            {
                tx_log_message(ix);
                ix = 1;
                blebuffer[0] = 0;
            }
        }
    }
    if(blebuffer[0] != 0) //buffer has messages that were not sent
    {
        tx_log_message(ix);
        blebuffer[0] = 0;
    }
    has_ct_error = false;
}

static void send_wdt_timeouts(void)
{
    blebuffer[0] = LOG_WDT;
    if(sizeof(wdt_timeout) < (sizeof(blebuffer)-1))
    {
        memcpy(&blebuffer[1], wdt_timeout, sizeof(wdt_timeout));
        tx_log_message(sizeof(wdt_timeout)+1);
        memset(wdt_timeout, 0xFF, sizeof(wdt_timeout));
    }
    else
    {
        diag_inc_flag(FLG_WDT_TIMEOUT);
        log_error("[sys] send WDT timeouts");
    }
    blebuffer[0] = 0;
}

static void send_modinfo(void)
{
    uint16_t data2b;
    uint32_t data4b;
    //message type
    blebuffer[0] = LOG_MODINFO;
    //module #
    data4b = 0; //<<todo>> put module serial # here
    memcpy(&blebuffer[1], &data4b, sizeof(data4b));
    //firmware version
    blebuffer[5] = FWV_MAJOR;
    blebuffer[6] = FWV_MINOR;
    blebuffer[7] = FWV_PATCH;
    blebuffer[8] = FWV_BUILD;
    //last reset cause
    memcpy(&blebuffer[9], &last_rst_reason, sizeof(last_rst_reason));
    data2b = ble_get_conn_interval();
    memcpy(&blebuffer[11], &data2b, sizeof(data2b));
    tx_log_message(13);
    blebuffer[0] = 0;
}

void diag_dump(diagnostic_id diags)
{
    TaskHandle_t hnd = tsk_get_handle(TSK_DIAGNOSTICS);
    if(NULL != hnd)
    {
        dump_diagnostic = diags;
        xTaskAbortDelay(hnd);
    }
}

#define MIN_INTERVAL_MS     10000
static void tsk_diagnostics(void *params)
{
    TickType_t delta;
    dump_diagnostic = 0;
    log_debug("last rst flag: 0x%04X\r\n", last_rst_reason);

    while(1)
    {
        //wait for BLE connection to send 1st diagnostic messages
        while(!ble_is_notification_on(BLEMSG_LOGGER))
        {
            dump_diagnostic = 0;
            wdt_enable(wdt, false);
            os_evt_wait(EVT_BLE_CCCD_WRITE, WAIT_FOREVER);
            wdt_enable(wdt, true);
            if(ble_is_notification_on(BLEMSG_LOGGER) && !dump_diagnostic)
            {
            	log_debug("logger first sent\r\n");
            	ble_clean_push_queue();
                send_modinfo();
                vTaskDelay(pdMS_TO_TICKS(50));
                send_tasks_stack(DIAG_STACK_ALL, true);
                vTaskDelay(pdMS_TO_TICKS(50));
                send_error_counters(DIAG_ERRORS);
                vTaskDelay(pdMS_TO_TICKS(50));
                if(send_wdt_timeout)
                {
                    send_wdt_timeouts();
                    vTaskDelay(pdMS_TO_TICKS(50));
                    send_wdt_timeout = false;
                }
                //reset counter block
                memset(ctblock, 0, sizeof(ctblock));
            }
        }
        //check diagnostic dump command received from BLE
        if(dump_diagnostic)
        {
            if(ble_is_notification_on(BLEMSG_LOGGER))
            {
                ble_clean_push_queue();
                send_modinfo();
                send_tasks_stack(dump_diagnostic, false);
                send_error_counters(dump_diagnostic);
            }
            dump_diagnostic = 0;
        }
        //periodic routine to check if it is necessary to send log messages
        vTaskDelay(pdMS_TO_TICKS(15000));
        wdt_feed(wdt);
        delta = xTaskGetTickCount()-last_sent;
#if ENABLE_CHECK_STACK == 1
		os_update_stack_level(); //this call takes ~2.5ms to complete
#endif //ENABLE_CHECK_STACK
        if(!dump_diagnostic && (ticks_to_ms(delta) >= MIN_INTERVAL_MS))
        {
        	if(ble_is_notification_on(BLEMSG_LOGGER) &&
              (has_ct_error || os_is_stack_warning()))
            {
                send_modinfo();
                if(has_ct_error)
                    send_error_counters(DIAG_ERRORS);
                if(os_is_stack_warning())
                    send_tasks_stack(DIAG_STACK, false);
            }
        }
    }
}

void HardFault_Handler(void)
{
    reset_mark = HARDFAULT_TAG;
    __DSB();
    SCB->AIRCR  = (uint32_t)((0x5FAUL << SCB_AIRCR_VECTKEY_Pos)    |
                             (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) |
                              SCB_AIRCR_SYSRESETREQ_Msk);
    __DSB();
    while(1)
        __NOP();
}
