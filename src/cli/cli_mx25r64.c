/*
 * cli_tests.c
 *
 *  Created on: Aug 24, 2016
 *      Author: Myant
 */

#include <string.h>
#include "cli_mx25r64.h"
#include "mx25r6435.h"
#include "mya_util.h"
#include "mya_str.h"
#include "logger.h"
#include "hal_gpio.h"
#include "tskctrl.h"
#include "memory.h"

#if ENABLE_CLI == 0
uint16_t cli_mx2564_get_commands(const CliCommand **commands){return 0;}
void cli_mx2564_start_profile(void){}
void cli_mx2564_pwr_down(void){}
void cli_mx2564_pwr_up(void){}
#else

#define MAX_BLOCK_LEN   20

#define HELP_DUMP       "Read block of data. Use: <hex addr> <nbytes>\r\n"
#define HELP_READ       "Read 4 bytes from memory. Use: <hex addr>\r\n"
#define HELP_WRITE      "Write memory.\r\n"
#define HELP_READ_ID    "Read memory id info\r\n"
#define HELP_READ_ST    "Read memory status\r\n"
#define HELP_TRACE      "Prints SPI tx and rx buffers. Use: trace <on/off>\r\n"
#define HELP_ERASE_SEC  "Erase 4Kb sector. Use: esector <hex addr>\r\n"
#define HELP_WR_BLOCK   "Write a data block starting at a given address.\r\n \
Use: wrblock <hex addr> <hex_data> <nbytes> (Max nbytes is 4096)\r\n"
#define HELP_PROFILE    "Execute pre-defined operations to measure time and \
current. Use: profile <# iterations>\r\n"
#define HELP_PWR_DOWN   "Control power down mode. Use pwrd <on/off>\r\n"
#define HELP_WRRD		"Write/read bytes\r\n"

static void cmd_dump(char *argv[], uint8_t argc);
static void cmd_read(char *argv[], uint8_t argc);
static void cmd_write(char *argv[], uint8_t argc);
static void cmd_read_id(char *argv[], uint8_t argc);
static void cmd_read_status(char *argv[], uint8_t argc);
static void cmd_trace(char *argv[], uint8_t argc);
static void cmd_erase_sector(char *argv[], uint8_t argc);
static void cmd_write_block(char *argv[], uint8_t argc);
static void cmd_profile(char *argv[], uint8_t argc);
static void cmd_pwr_down(char *argv[], uint8_t argc);
static void cmd_wrrd(char *argv[], uint8_t argc);
static inline void timeit_start(void);
static inline TickType_t timeit_stop(bool print);
static inline bool wait_operation(TickType_t timeout);

static const CliCommand test_commands[] =
{
    {"dump",    HELP_DUMP,      cmd_dump},
    {"read",    HELP_READ,      cmd_read},
    {"write",   HELP_WRITE,     cmd_write},
    {"esector", HELP_ERASE_SEC, cmd_erase_sector},
    {"id",      HELP_READ_ID,   cmd_read_id},
    {"status",  HELP_READ_ST,   cmd_read_status},
    {"trace",   HELP_TRACE,     cmd_trace},
    {"wrblock", HELP_WR_BLOCK,  cmd_write_block},
    {"profile", HELP_PROFILE,   cmd_profile},
    {"pwrd",    HELP_PWR_DOWN,  cmd_pwr_down},
	{"wrrd",	HELP_WRRD, 		cmd_wrrd},
};
static const uint16_t NTEST_CMD = sizeof(test_commands)/sizeof(CliCommand);
//buffer to be used by block operations
static uint8_t block_data[MAX_BLOCK_LEN] = {0};
static TickType_t start_time = 0;

uint16_t cli_mx2564_get_commands(const CliCommand **commands)
{
    *commands = test_commands;
    return NTEST_CMD;
}

static inline void timeit_start(void)
{
    start_time = xTaskGetTickCount();
    hal_gpio_clr(LED_R);
}

static inline TickType_t timeit_stop(bool print)
{
    hal_gpio_set(LED_R);
    TickType_t elapsed = xTaskGetTickCount()-start_time;
    if(print)
        cli_print("\r\nop time: %d ms", elapsed);
    return elapsed;
}

static inline bool wait_operation(TickType_t timeout)
{
    TickType_t diff, start = xTaskGetTickCount();
    do
    {
        mem_read_status(NULL);
        diff = xTaskGetTickCount()-start;
        if(diff >= timeout)
            return false;
    }while(mem_is_busy());
    return true;
}

static void cmd_trace(char *argv[], uint8_t argc)
{
    check_args(argc, 2);
    if(str_equal(argv[1], "on", 4))
        mem_set_trace(true);
    else if(str_equal(argv[1], "off", 4))
        mem_set_trace(false);
    else
        cli_print("\r\nWrong argument");
}

static void cmd_read_id(char *argv[], uint8_t argc)
{
    uint16_t id;
    memory_status_t st ;
    st=mem_get_flash_id(&id);
    //will change to gatekeeper later
    //st =MEMST_FAIL;
    if(MEMST_OK == st)
        cli_print("\r\nmemory id: 0x%04X", id);
    else
        cli_print("\r\nread id error: %d", st);
}

static void cmd_read_status(char *argv[], uint8_t argc)
{
    uint8_t memst;
    memory_status_t st;
    st= mem_read_status(&memst);
    //will change to gatekeeper later
    //st =MEMST_FAIL;
    if(MEMST_OK == st)
        cli_print("\r\nmemory status: 0x%04X", memst);
    else
        cli_print("\r\nread status error: %d", st);
}

static void cmd_dump(char *argv[], uint8_t argc)
{
    check_args(argc, 3);
    uint64_t tmp64 = 0;
    uint32_t addr32, i, k;
    uint16_t block_len;
    memory_status_t memst;

    //read address
    if(!str_2uint(argv[1], &tmp64, 16))
    {
        cli_print("\r\nWrong address. Hex base must be used");
        return;
    }
    addr32 = (uint32_t)tmp64;
    //how many bytes must be read
    if(!str_2uint(argv[2], &tmp64, 10))
    {
        cli_print("\r\nWrong data. Decimal base must be used");
        return;
    }
    if(tmp64 > MAX_BLOCK_LEN)
    {
        cli_print("\r\nError. Max block size is: %ld", MAX_BLOCK_LEN);
        return;
    }
    block_len = (uint16_t)tmp64;
    memset(block_data, 0xFF, MAX_BLOCK_LEN);
    timeit_start();
    memst = mem_read(addr32, block_data, block_len);
    if(MEMST_OK == memst)
    {
        if(wait_operation(pdMS_TO_TICKS(10000)))
            timeit_stop(true);
        else
            cli_print("\r\nread operation timed out");
    }
    else
    {
        cli_print("\r\nMemory read error: %d", memst);
        return;
    }
    //print header
    cli_print("\r\n  ADDR    00  01  02  03  04  05  06  07  08  09"
              "  0A  0B  0C  0D  0E  0F");
    cli_print("\r\n------------------------------------------------------"
              "------------------");
    for(i = 0; i < block_len; )
    {
        cli_print("\r\n%08X  ", i+addr32);
        for(k = 0; k < 16; k++, i++)
            cli_print("%02X  ", block_data[i]);
        if((i % 32) == 0)
            vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void cmd_read(char *argv[], uint8_t argc)
{
    check_args(argc, 2);
    uint64_t addr64 = 0;
    uint32_t content, addr32;
    if(!str_2uint(argv[1], &addr64, 16))
    {
        cli_print("\r\nWrong address. Hex base must be used");
        return;
    }
    addr32 = (uint32_t)addr64;
    memory_status_t st = mem_read(addr32, &content, 4);
    if(MEMST_OK == st)
        cli_print("\r\n[0x%08X] = 0x%08X", addr32, content);
    else
        cli_print("\r\nRead error: %d", st);
}

static void cmd_write(char *argv[], uint8_t argc)
{
    check_args(argc, 3);
    uint64_t tmp64 = 0;
    uint32_t content, addr32;
    //read address
    if(!str_2uint(argv[1], &tmp64, 16))
    {
        cli_print("\r\nWrong address. Hex base must be used");
        return;
    }
    addr32 = (uint32_t)tmp64;
    //read content
    if(!str_2uint(argv[2], &tmp64, 16))
    {
        cli_print("\r\nWrong data. Hex base must be used");
        return;
    }
    //write
    content = (uint32_t)tmp64;
    memory_status_t st = mem_write(addr32, &content, 4);
    if(MEMST_OK == st)
        cli_print("\r\nwritten [0x%08X] = 0x%08X", addr32, content);
    else
        cli_print("\r\nWrite error: %d", st);
}

static uint8_t buff1[256];
static uint8_t buff2[256];
static void cmd_wrrd(char *argv[], uint8_t argc)
{
	uint32_t addr = 0;
	memory_status_t st;
	memset(buff1, 0x77, sizeof(buff1));
	memset(buff2, 0x00, sizeof(buff2));
	cli_print("\r\nWriting addr[0x%08X]", addr);
	st = mem_write(addr, buff1, sizeof(buff1));
	if(MEMST_OK == st)
	{
		util_blocking_delay_ms(10);
		st = mem_read(addr, buff2, sizeof(buff2));
		if(MEMST_OK == st)
		{
			if(memcmp(buff1, buff2, sizeof(buff1)) == 0)
				cli_print("\r\nOperation successful!");
			else
				cli_print("\r\nError. Read data does not match written data");
		}
		else
		{
			cli_print("\r\nRead operation failed: %d", st);
			return;
		}
	}
	else
	{
		cli_print("\r\nWrite operation failed: %d", st);
	}
}

static void cmd_write_block(char *argv[], uint8_t argc)
{
    check_args(argc, 4);
    uint64_t tmp64 = 0;
    uint32_t addr32;
    uint8_t byte;
    uint16_t block_len;
    //read address
    if(!str_2uint(argv[1], &tmp64, 16))
    {
        cli_print("\r\nWrong address. Hex base must be used");
        return;
    }
    addr32 = (uint32_t)tmp64;
    //read content
    if(!str_2uint(argv[2], &tmp64, 16))
    {
        cli_print("\r\nWrong data. Hex base must be used");
        return;
    }
    if(tmp64 > 0xFF)
    {
        cli_print("\r\nWrong data. %s is greater than 1 byte!", argv[2]);
        return;
    }
    byte = (uint8_t)tmp64;
    //how many bytes must be written
    if(!str_2uint(argv[3], &tmp64, 10))
    {
        cli_print("\r\nWrong data. Decimal base must be used");
        return;
    }
    if(tmp64 > MAX_BLOCK_LEN)
    {
        cli_print("\r\nError. Max block size is: %ld", MAX_BLOCK_LEN);
        return;
    }
    block_len = (uint16_t)tmp64;
    //write
    memset(block_data, byte, block_len);
    timeit_start();
    memory_status_t st = mem_write(addr32, block_data, block_len);
    if(MEMST_OK == st)
    {
        if(wait_operation(pdMS_TO_TICKS(10000)))
            timeit_stop(true);
        else
            cli_print("\r\nwrite block timed out");
        cli_print("\r\nBlock written");
    }
    else
        cli_print("\r\nWrite error: %d", st);
}

static void cmd_erase_sector(char *argv[], uint8_t argc)
{
    check_args(argc, 2);
    uint64_t addr64 = 0;
    uint32_t addr32;
    if(!str_2uint(argv[1], &addr64, 16))
    {
        cli_print("\r\nWrong address. Hex base must be used");
        return;
    }
    addr32 = (uint32_t)addr64;
    timeit_start();
    memory_status_t st = mem_erase_page(addr32);
    if(MEMST_OK == st)
    {
        if(wait_operation(pdMS_TO_TICKS(10000)))
            timeit_stop(true);
        else
            cli_print("\r\nsector erased timed out");
        cli_print("\r\nsector erased");
    }
    else
        cli_print("\r\nerase sector error: %d", st);
}

#define PROFILE_READ    1
#define PROFILE_WRITE   0
#define PROFILE_ERASE   0
static void cmd_profile(char *argv[], uint8_t argc)
{
    check_args(argc, 2);
    uint64_t it64 = 0;
    uint32_t itn;
    if(!str_2uint(argv[1], &it64, 10))
    {
        cli_print("\r\nWrong iterations #. Decimal base must be used");
        return;
    }
    memory_status_t st;
    uint32_t i, addr = 0;
    uint16_t len = 256;
    itn = (uint32_t)it64;
    cli_print("\r\nprofiling with %d iterations...", itn);
    TickType_t start = xTaskGetTickCount();
    hal_gpio_clr(LED_R);
    for(i = 0; i < itn; i++)
    {
#if PROFILE_READ == 1
        mem_read_status(NULL);
        while(mem_is_busy())
            mem_read_status(NULL);
        st = mem_read(addr, block_data, len);
        if(MEMST_OK != st)
        {
            log_error("Read error %d at iteration %d", st, i);
            break;
        }
#endif //PROFILE_READ
#if PROFILE_ERASE == 1
        do
        {
            st = mem_erase_page(addr);
        }while(MEMST_BUSY == st);
        if(MEMST_OK != st)
        {
            log_error("Erase error %d at iteration %d", st, i);
            break;
        }
        addr += 0x1000;
        if(addr > 0x7FF00)
            break;
#endif //PROFILE_ERASE
#if PROFILE_WRITE == 1
        mem_read_status(NULL);
        while(mem_is_busy()) //wait erase operation
            mem_read_status(NULL);
        do
        {
            st = mem_write(addr, block_data, len);
        }while(MEMST_BUSY == st);
        if(MEMST_OK != st)
        {
            log_error("Write error %d at iteration %d", st, i);
            break;
        }
        addr += 256;
#endif //PROFILE_WRITE
    }
    hal_gpio_set(LED_R);
    cli_print("\r\ntime: %d ms", (xTaskGetTickCount()-start));
}

static void cmd_pwr_down(char *argv[], uint8_t argc)
{
    check_args(argc, 2);
    memory_status_t st = MEMST_OK;
    if(str_equal(argv[1], "on", 4))
        st = mem_power_down();
    else if(str_equal(argv[1], "off", 4))
        st = mem_power_up();
    else
        cli_print("\r\nWrong argument");
    if(MEMST_OK != st)
        cli_print("\r\nPower down cmd error: %d", st);
}

void cli_mx2564_start_profile(void)
{
    char *command[] = {"profile","10000"};
    cmd_profile(command, 2);
}

void cli_mx2564_pwr_down(void)
{
    mem_power_down();
}

void cli_mx2564_pwr_up(void)
{
    mem_power_up();
}
#endif //ENABLE_CLI
