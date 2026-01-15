/*
 * gtk_spi.c
 *
 *  Created on: Sep 20, 2016
 *      Author: Myant
 */

#include "gtk_spi.h"
#if GTK_SPI_ON == 1

#include "hal_spi.h"
#include "logio.h"
#include "appconfig.h"
#include "tskctrl.h"
#include "logger.h"
//#include "diagnostic.h"
#include "diagnostic.h"
#include "wdt.h"

//tx queue definitions
#define QTXLENGTH       10
#define TIME2TX         500 //time (in ms) to wait for a tx notification
#define TIME2ENQUEUE    200 //time (in ms) to enqueue data into tx buffer

typedef struct
{
    uint8_t *txdata;    //reference to bytes to transmit
    uint8_t *rxdata;    //reference to bytes to receive
    uint16_t len;       //data length in bytes
    IOPin cs;           //chip select
    TaskHandle_t taskn; //task to be notified when transmission is finished
}QData;

static void irq_spi_tx(void);
static void config_spi(void);
static void tsk_gtk_spi_tx(void *params);

static spi_port_t spi = NULL;
static SPIStatus spi_status;
static bool configured = false;
static QueueHandle_t txq = NULL;
static TaskHandle_t hnd_tsk_tx = NULL;
static uint32_t lentxbuffer = 0;
static wdt_entry wdt = NULL;

static void config_spi(void)
{
    spi = hal_spi_init(SPI_ID1);
    if(spi)
    {
        spi_status = hal_spi_set_tx_callback(spi, irq_spi_tx);
        if(SPIST_OP_OK == spi_status)
        {
            hal_spi_enable_irq_tx(spi, true);
            hal_spi_enable(spi, true);
            lentxbuffer = hal_spi_len_tx_buffer(spi);
        }
    }
}

bool gtk_spi_init(void)
{
    if(!configured)
    {
        config_spi();
        //create tx queue
        txq = os_create_queue(QTXLENGTH, sizeof(QData));
        ASSERT_QUEUE(txq);
        if(SPIST_OP_OK == spi_status)
        {
            hnd_tsk_tx = tsk_create(TSK_GTK_SPI, tsk_gtk_spi_tx);
            ASSERT_TASK(hnd_tsk_tx);
            //config watchdog timer
            wdt_config_t wconfig;
            wconfig.id = TSK_GTK_SPI;
            wconfig.enabled = true;
            wconfig.event_mask = 0;
            wconfig.timeout = 2;
            wconfig.type = WDT_E_BLOCKING;
            wdt = wdt_config_entry(&wconfig, pdMS_TO_TICKS(200));
        }
        configured = (SPIST_OP_OK == spi_status);
    }
    return configured;
}

void gtk_spi_deinit(void)
{
	if(!configured)
		return;
	configured = false;
	tsk_suspend_by_id(TSK_GTK_SPI);
	xQueueReset(txq);
	hal_spi_deinit(SPI_ID1);
	return;
}

bool gtk_spi_tx(uint8_t *txd, uint8_t *rxd, uint16_t len, IOPin cs,
                TaskHandle_t *tsk)
{
    QData data;
    BaseType_t enq = pdPASS;
    //check invalid conditions
    if(!configured || txd == NULL || len == 0 ||
       lentxbuffer == 0 || len > lentxbuffer)
        return false;
    //is queue full ?
    if(QTXLENGTH == uxQueueMessagesWaiting(txq))
    {
    	diag_inc_flag(FLG_GTKSPI_TX_SPACE);
        log_error("gtk spi - no tx buffer space");
        return false;
    }

    data.txdata = txd;
    data.rxdata = rxd;
    data.len = len;
    data.cs = cs;
    data.taskn = tsk;
    wdt_unblock_task(wdt);
    enq = xQueueSendToBack(txq, &data, pdMS_TO_TICKS(TIME2ENQUEUE));
    if(pdPASS != enq)
    {
        wdt_feed(wdt);
        diag_inc_flag(FLG_GTKSPI_ENQUEUE_ERROR);
    }

    return (pdPASS == enq);
}
//<<todo: Increase the data size to 256 Bytes, Currently SPI can read 255 bytes>>
static void tsk_gtk_spi_tx(void *params)
{
    QData data;
    BaseType_t status;
    uint32_t ntfvalue;
    while(1)
    {
        status = xQueueReceive(txq, &data, WAIT_FOREVER);
        wdt_feed(wdt);
        if(pdPASS == status)
        {
            if(!hal_spi_is_enabled(spi))
                hal_spi_enable(spi, true);
            if(data.cs != IGNORE_CS_PIN)
                hal_gpio_clr(data.cs);
            hal_spi_tx(spi, data.txdata, data.rxdata, data.len);
            //block until tx finishes
            ntfvalue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TIME2TX));
            if(data.cs != IGNORE_CS_PIN)
                hal_gpio_set(data.cs);
            if(0 == ntfvalue) //notification didnt arrive on expected time
            	diag_inc_flag(FLG_GTKSPI_TXTIMEOUT);
            else if(data.taskn)
                xTaskNotifyGive(data.taskn);
            if(0 == uxQueueMessagesWaiting(txq))
                hal_spi_enable(spi, false);
        }
        else
        {
        	diag_inc_flag(FLG_GTKSPI_DEQUEUE_TX);
        }
    }
}

bool gtk_is_configured(void)
{
	return configured;
}

static void irq_spi_tx(void)
{
    BaseType_t context_switch = pdFALSE;
    vTaskNotifyGiveFromISR(hnd_tsk_tx, &context_switch);
    portYIELD_FROM_ISR(context_switch);
}

#endif //GTK_SPI_ON
