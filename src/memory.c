/*
 * memory.c
 *
 *  Created on: Mar 11, 2021
 *      Author: Jeffrey Zhu
 */
#include "memory.h"
#include "mx25r6435.h"
#include "gtk_spi.h"
#include <string.h>
#include "hal_gpio.h"
#include "tskctrl.h"
#include "diagnostic.h"
#include "logger.h"
#include <util/mya_util.h>
#include "cli.h"
#include "hal_spi.h"


#define LEN_SPI_BUFFER    255
static uint8_t txdata[LEN_SPI_BUFFER] = {0};
static uint8_t rxdata[LEN_SPI_BUFFER] = {0};

static memory_status_t mem_reset(void);
static bool memory_initialized = false;
static bool memory_powered_down = false;

// ################ functions to interface with mx25r6435.c driver

static bool _spi_tx(uint8_t *txd, uint8_t ntx, uint8_t *rxd, uint8_t nrx,
                         uint8_t cs)
{
    uint32_t ntfvalue;
    uint16_t len;
    int8_t success = true;

    len = (ntx > nrx) ? ntx : nrx;
    memset(txdata, 0, len);
    memset(rxdata, 0, len);
    memcpy(txdata, txd, ntx);
    memcpy(rxdata, rxd, nrx);
    ulTaskNotifyTake(pdTRUE, DONT_WAIT);
    if(gtk_spi_tx(txdata, rxdata, len, cs, self_task()))
    {
        ntfvalue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
        if(0 == ntfvalue)
        {
        	diag_inc_flag(FLG_MEM_SPI_TIMEOUT);
            log_error("[MEM] SPI tx timed out!");
            success = false;
        }
        else if(nrx)
        {
        	memcpy(rxd, rxdata, nrx);
        }
    }
    else
    {
    	diag_inc_flag(FLG_MEM_SPI_GTKERROR);
        log_error("[MEM] SPI gatekeeper error!");
        success = false;
    }
    return success;
}


static void _delay_ms(uint32_t period)
{
    vTaskDelay(pdMS_TO_TICKS(period));
}

static void _clear_pin(IOPin pin)
{
	hal_gpio_clr(pin);
}

static void _set_pin(IOPin pin)
{
	hal_gpio_set(pin);
}

bool memory_init(void)
{
	mx_hw_t mxhw;
	memory_status_t st;

	if(memory_initialized)
		return true;

	mxhw.spi_tx = _spi_tx;
	mxhw.delay_ms = _delay_ms;
	mxhw.cs = CS_MEM;
	mxhw.pin_clr = _clear_pin;
	mxhw.pin_set = _set_pin;

	mem_reset();

	if(gtk_is_configured())
	{
		st = mx_init(&mxhw);
		if(st == MEMST_OK)
		{
			memory_initialized = true;
			return true;
		}
	}
	return false;
}

#if CLI_SUPPORT_ON == 1
memory_status_t mem_set_trace(bool enabled)
{
	mx_set_trace(enabled);
	return MEMST_OK;
}
#endif

bool mem_is_busy(void)
{
	return  mx_is_busy();
}

memory_status_t mem_get_flash_id(uint16_t *id)
{
	if(id==NULL)
		return false;
#if BYPASS_MEMORY == 1
	*id=MXID;
	return true;
#else
    memory_status_t st;

    if(!memory_init())
    {
    	log_error(">> fail to init memory <<\r\n");
    	return MEMST_FAIL;
    }
    mem_power_up();
    st = mx_read_id(id);
    st = mem_power_down();

    if(MEMST_OK != st)
    {
        log_error("[memory] failed to read memory id %d", st);
        *id = 0;
        return st;
    }
    else
    	log_debug("memory id= %x \n", *id);

    return st;
#endif
}


memory_status_t mem_read(uint32_t addr, void *data, uint16_t len)
{
	//todo - Need to check/verify the maximum number of bytes that can be
	//		read from memory IC (MX25R6435F) over SPI.
	//		MX25R6435F Data sheet (page 36) suggests that the whole memory
	//      can be read in a single instruction.
	//		LEN_SPI_BUFFER parameter to be set accordingly

//	if(len>255)			// Temporary check added, causing error in reading pcb
//		return MEMST_LENGTH_ERROR;		//temperature from ADS IC


    memory_status_t st;

    if(!memory_init())
    {
       	log_error(">> fail to init memory <<\r\n");
       	return MEMST_FAIL;
    }
    mem_power_up();
    st = mx_read(addr,data,len);
    st = mem_power_down();

    if(MEMST_OK != st)
    {
    	log_error("[memory] failed to read data, error = %d", st);
        return st;
    }

    return st;
}

memory_status_t mem_write(uint32_t addr, void *data, uint16_t len)
{
    memory_status_t st;

    if(!memory_init())
    {
       	log_error(">> fail to init memory <<\r\n");
       	return MEMST_FAIL;
    }
    mem_power_up();
    st = mx_write(addr,data,len);
    st = mem_power_down();

    if(MEMST_OK != st)
    {
    	log_error("[memory] failed to write data, error = %d", st);
        return st;
    }

    return st;
}

memory_status_t mem_read_status(uint8_t *status)
{
	memory_status_t st;

	if(!memory_init())
	{
	   	log_error(">> fail to init memory <<\r\n");
	  	return MEMST_FAIL;
	}
	mem_power_up();
	st = mx_read_status(status);
	st = mem_power_down();
	if(MEMST_OK != st)
	{
	  	log_error("[memory] failed to read status, error = %d", st);
	    return st;
	}

	return st;
}

memory_status_t mem_read_page(uint16_t page, uint32_t addr, void *data, uint16_t len)
{
	memory_status_t st;

	if(!memory_init())
	{
	  	log_error(">> fail to init memory <<\r\n");
	   	return MEMST_FAIL;
	}
	mem_power_up();
    st = mx_read_page(page,addr,data,len);
    st = mem_power_down();
    if(MEMST_OK != st)
    {
    	log_error("[memory] failed to read page, error = %d", st);
    	return st;
    }

    return st;
}

memory_status_t mem_write_page(uint16_t page, uint16_t addr, void *data, uint16_t len)
{
	memory_status_t st;

	if(!memory_init())
	{
	  	log_error(">> fail to init memory <<\r\n");
	   	return MEMST_FAIL;
	}
	mem_power_up();
	st = mx_write_page(page,addr,data,len);
	st = mem_power_down();
	if(MEMST_OK != st)
	{
	  	log_error("[memory] failed to write page, error = %d", st);
	   	return st;
	}

    return st;
}

memory_status_t mem_erase_page(uint32_t addr)
{
	memory_status_t st;

	if(!memory_init())
	{
	  	log_error(">> fail to init memory <<\r\n");
	   	return MEMST_FAIL;
	}
	mem_power_up();
	st = mx_erase_page(addr);
	st = mem_power_down();
	if(MEMST_OK != st)
	{
		log_error("[memory] failed to erase page, error = %d", st);
		return st;
	}

	return st;
}

memory_status_t mem_power_up(void)
{
	if(!memory_powered_down)
		return MEMST_OK;
    //wake the device up
    hal_gpio_clr(CS_MEM);
    util_blocking_delay_us(350);
    hal_gpio_set(CS_MEM);
    util_blocking_delay_us(350);
    memory_powered_down = false;

    return MEMST_OK;
}

memory_status_t mem_power_down(void)
{
	 memory_status_t st;

	 st = mx_power_down();
	 if(MEMST_OK != st)
	 {
	 	log_error("[memory] failed to power down, error = %d", st);
	 	return st;
	 }
	 memory_powered_down = true;
	 return st;
}

static memory_status_t mem_reset(void)
{
    hal_gpio_set(MEM_RST);
    hal_gpio_clr(MEM_RST);
    util_blocking_delay_us(50);
    hal_gpio_set(MEM_RST);
    util_blocking_delay_us(50);
    return MEMST_OK;
}
