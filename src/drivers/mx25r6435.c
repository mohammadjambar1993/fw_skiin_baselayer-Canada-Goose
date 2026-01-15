/*
 * mx25r6435.c
 *
 *  Created on: Sep 20, 2017
 *      Author: Myant
 */

#include <string.h>
#include "mx25r6435.h"

#include "../diagnostic.h"
#include "gtk_spi.h"
#include "hal_gpio.h"
#include "tskctrl.h"
#include "logger.h"
#include "mya_util.h"
#include "cli.h"
#include "hal_spi.h"
#include "hal_gpio.h"

#define FAST_READ       1
#define MAX_WRITE_LEN   256
#define LAST_ADDR       0x7FFFFF
#define MEMORY_ID       0xC228
#define PAGE_LEN        4096     //in datasheet, it is called 'sector'
#define NPAGES          2048
#define BLOCK_LEN       256      //in datasheet, it is called 'page'

#define NRX_READ_ID       	 3
#define NRX_STATUS_REG       2
#define NRX_WRITE_EN         0

//commands list
#define CMD_PAGE_PROGRAM    0x02
#define CMD_READ_BYTES      0x03
#define CMD_WR_DISABLE      0x04
#define CMD_READ_STATUS     0x05
#define CMD_WR_ENABLE       0x06
#define CMD_FAST_READ       0x0B
#define CMD_ERASE_SECTOR    0x20
#define CMD_READ_ID         0x9F
#define CMD_PWRDOWN         0xB9

//macros to extract information from status register
#define SR_SRWD(x)  (x & 0x80)
#define SR_BP(x)    ((x & 0x3C)>>2)
#define SR_WEL(x)   ((x & 0x02)>>1)
#define SR_WIP(x)   (x & 0x01)

static bool read_status_reg(void);
static memory_status_t write_enable(bool enable);
//static void irq_spi_tx(void);

//print all spi transmitted and received bytes
static bool trace_spi = false;

static uint8_t status_reg;      //status register

static mx_hw_t mx_fptr;
uint8_t rx_data[BLOCK_LEN-1] = {0};

static inline void print_trace(void *tx, void *rx, uint16_t len, bool new)
{
#if CLI_SUPPORT_ON == 1
    if(!trace_spi)
        return;
    static uint32_t last_ix = 0;
    uint8_t *txdata = (uint8_t*)tx;
    uint8_t *rxdata = (uint8_t*)rx;
    uint16_t i;
    uint8_t nullchar = 0;

    if(NULL == txdata)
        txdata = &nullchar;
    if(NULL == rxdata)
        rxdata = &nullchar;
    if(new)
    {
        last_ix = 0;
        cli_print("\r\nspi trace\r\n");
        cli_print("byte    tx      rx\r\n");
    }
    for(i = 0; i < len; i++)
    {
        cli_print("[%d]    0x%02X    0x%02X\r\n", i+last_ix, *txdata, *rxdata);
        if(txdata)
            txdata++;
        if(rxdata)
            rxdata++;
    }
    last_ix = i;
#endif //CLI_SUPPORT_ON
}

static bool read_status_reg(void)
{
    uint8_t st[2];
    uint8_t reg_addr = CMD_READ_STATUS;
    if(mx_fptr.spi_tx(&reg_addr,sizeof(reg_addr),st,NRX_STATUS_REG,mx_fptr.cs))
    {
        status_reg = st[1];
        return true;
    }
    return false;
}

static memory_status_t write_enable(bool enable)
{
    if(!read_status_reg())
        return MEMST_FAIL;
    if(SR_WIP(status_reg))
        return MEMST_BUSY;
    if(!SR_WEL(status_reg))
    {
        uint8_t reg_addr = enable ? CMD_WR_ENABLE : CMD_WR_DISABLE;
        if(!mx_fptr.spi_tx(&reg_addr,sizeof(reg_addr),NULL,NRX_WRITE_EN,mx_fptr.cs))
        	return MEMST_SPI_ERROR;
    }
    return MEMST_OK;
}

memory_status_t mx_init(mx_hw_t *hw)
{
    uint16_t id = 0;
    bool initok = false;
    if(hw == NULL)
    	return false;

    memcpy(&mx_fptr,hw,sizeof(mx_hw_t));

    initok  = read_status_reg();
    initok &= (MEMST_OK == mx_read_id(&id));
    initok &= (MEMORY_ID == id);
    if(initok)
        return MEMST_OK;

    return MEMST_FAIL;
}


#if CLI_SUPPORT_ON == 1
memory_status_t mx_set_trace(bool enabled)
{
    trace_spi = enabled;
    return MEMST_OK;
}
#endif //CLI_SUPPORT_ON

memory_status_t mx_read_id(uint16_t *id)
{
    if(NULL == id)
        return MEMST_INVALID_PARAM;

    uint8_t devid[3];
    uint8_t reg_addr=CMD_READ_ID;

    if(mx_fptr.spi_tx(&reg_addr,sizeof(reg_addr),devid,NRX_READ_ID,mx_fptr.cs))
    {
    	*id = (devid[1]<<8)+devid[2];
    	return MEMST_OK;
    }

    return MEMST_FAIL;
}

memory_status_t mx_read_status(uint8_t *status)
{
    if(read_status_reg())
    {
        if(NULL != status)
            *status = status_reg;
        return MEMST_OK;
    }
    return MEMST_FAIL;
}

bool mx_is_busy(void)
{
    return SR_WIP(status_reg);
}

static inline void set_address(uint8_t *buffer, uint32_t addr)
{
    *buffer = (uint8_t)((addr & 0x00FF0000)>>16); buffer++;
    *buffer = (uint8_t)((addr & 0x0000FF00)>>8); buffer++;
    *buffer = (uint8_t)(addr & 0x000000FF);
}

memory_status_t mx_read_page(uint16_t page, uint32_t addr, void *data, uint16_t len)
{
    if((NULL == data) || (len == 0) || (page >= NPAGES))
        return MEMST_INVALID_PARAM;
    if((len > PAGE_LEN) || (addr>=PAGE_LEN))
        return MEMST_ADDR_ERROR;

    uint32_t addr32 = (page*PAGE_LEN)+addr;
    return mx_read(addr32, data, len);
}

#if FAST_READ == 1
#define READ_PAYLOAD    5
#define READ_OPCODE     CMD_FAST_READ
#else
#define READ_PAYLOAD    4
#define READ_OPCODE     CMD_READ_BYTES
#endif
memory_status_t mx_read(uint32_t addr, void *data, uint16_t len)
{
    if((NULL == data) || (len == 0))
        return MEMST_INVALID_PARAM;
    if((addr+len) > LAST_ADDR+1)
        return MEMST_ADDR_ERROR;

    memory_status_t memst = MEMST_FAIL;
    uint8_t tx_data[READ_PAYLOAD] = {0};

    tx_data[0] = READ_OPCODE;
    set_address(&tx_data[1], addr);
    mx_fptr.pin_clr(mx_fptr.cs);
    if(mx_fptr.spi_tx(tx_data,READ_PAYLOAD,NULL,0,IGNORE_CS_PIN))
    {
       if(mx_fptr.spi_tx(NULL,0,rx_data,sizeof(rx_data),IGNORE_CS_PIN))
       {
    	   memcpy(data,rx_data,len);
    	   memst = MEMST_OK;
       }
       else
           memst = MEMST_SPI_ERROR;
    }
    else
    	memst = MEMST_SPI_ERROR;
    mx_fptr.pin_set(mx_fptr.cs);
    return memst;
}

#define MAX_WAIT_CT 3
/*<<notice>>
 * From data sheet, the Page Program (PP) command requires that the last byte
 * of address be all zeros for 256 bytes page program. If the data bytes sent to
 * the device exceeds 256, the last 256 data byte is programmed at the request
 * page and previous data will be disregarded.
 * If the last address byte are not all zeros, transmitted data that exceed 256
 * bytes length are programmed from the starting address (24-bit address that
 * last 8 bit are all 0s) of currently selected page.
 * In our application, all the program operation is to from the starting address
 * of each sector (4KB) which must be multiple of 256.
 * The only exempt is the SAT sector update which requires last byte of address
 * to be multiple of 4 instead of 256.
 * So we still need the argument 'addr' for more accurate address control.*/
memory_status_t mx_write_page(uint16_t page, uint16_t addr, void *data, uint16_t len)
{
    if((NULL == data) || (len == 0) || (page >= NPAGES) || (addr >= PAGE_LEN))
        return MEMST_INVALID_PARAM;
    if (addr+len > PAGE_LEN)
    	return MEMST_ADDR_ERROR;
    if ((addr%BLOCK_LEN) && ((addr%BLOCK_LEN)+len > BLOCK_LEN))
    {
    	log_error("[MEM] addr err, addr:%d, len:%d", addr, len);
    	return MEMST_ADDR_ERROR;
    }

    uint16_t nbtx;
    memory_status_t st;
    uint8_t *buffer = (uint8_t*)data;
    uint32_t addr32 = page*PAGE_LEN + addr;
    uint8_t ctwait;
    bool flag, busy;
    while(len)
    {
        nbtx = (len > MAX_WRITE_LEN) ? MAX_WRITE_LEN : len;
        st = mx_write(addr32, buffer, nbtx);
        if(MEMST_OK != st)
            break;
        len -= nbtx;
        buffer += nbtx;
        addr32 += nbtx;
        if(nbtx) //wait operation to finish
        {
            ctwait = 0;
            busy = true;
            while(busy && (ctwait < MAX_WAIT_CT))
            {
                //time to write 1B is ~ 26us, or ~40B/ms
            	util_check_flag_us(&flag, ((nbtx/40)+1)*1000);
                ctwait++;
                mx_read_status(NULL);
                busy = mx_is_busy();
            }
            if(busy || ctwait >= MAX_WAIT_CT)
            {
                st = MEMST_BUSY;
                break;
            }
        }
    }
    return st;
}


#define WR_PAYLOAD  4 //1b opcode + 3b addr
memory_status_t mx_write(uint32_t addr, void *data, uint16_t len)
{
    if((NULL == data) || (len == 0))
        return MEMST_INVALID_PARAM;
    if(len > MAX_WRITE_LEN)
        return MEMST_LENGTH_ERROR;
    if((addr > LAST_ADDR) || ((addr+len-1) > LAST_ADDR))
        return MEMST_ADDR_ERROR;

    memory_status_t st = write_enable(true);
    if(MEMST_OK != st)
        return st;

    uint8_t tx_data[WR_PAYLOAD];
    tx_data[0] = CMD_PAGE_PROGRAM;
    set_address(&tx_data[1], addr);
    mx_fptr.pin_clr(mx_fptr.cs);
    if(mx_fptr.spi_tx(tx_data,WR_PAYLOAD,NULL,0,IGNORE_CS_PIN))
    {
       	if(mx_fptr.spi_tx(data,len,NULL,0,IGNORE_CS_PIN))
       		st = MEMST_OK;
       	else
       		st = MEMST_SPI_ERROR;
    }
    else
       	st = MEMST_SPI_ERROR;
    mx_fptr.pin_set(mx_fptr.cs);
    return st;
}

memory_status_t mx_erase_page(uint32_t addr)
{
    if(addr > LAST_ADDR)
        return MEMST_ADDR_ERROR;
    memory_status_t st = write_enable(true);
    if(MEMST_OK != st)
        return st;

    uint8_t tx_data[4];
    tx_data[0] = CMD_ERASE_SECTOR;
    set_address(&tx_data[1], addr);

    if(mx_fptr.spi_tx(tx_data,sizeof(tx_data),NULL,0,mx_fptr.cs))
       	return MEMST_OK;

    return MEMST_FAIL;
}


memory_status_t mx_power_down(void)
{
	/*TODO:power down call when mem was already powerd down hangs MOSI line to high.
	 * Added power down flag check below.
	 * Discuss with Tiago regarding this.
	 * How did the power down module work before and does not work now?
	 */

    uint8_t tx_data = CMD_PWRDOWN;

    if(mx_fptr.spi_tx(&tx_data,sizeof(tx_data),NULL,0,mx_fptr.cs))
    	return MEMST_OK;
    else
    	return MEMST_SPI_ERROR;

}

