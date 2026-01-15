/*
 * cli.c
 *
 *  Created on: Aug 23, 2016
 *      Author: Myant
 */

#include "adc_ctrl.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "appconfig.h"
#include "cli.h"
#include "system.h"
#include "../diagnostic.h"
#include "../drivers/ina231.h"
#include "../drivers/tps65987.h"
#include "uartshell.h"
#include "cli_mx25r64.h"
#include "task.h"
#include "tskctrl.h"
#include "hal_gpio.h"
#include "hal_ble.h"
#include "logger.h"
#include "mya_str.h"
#include "mx25r6435.h"
#include "wdt.h"
#include "ble_rpc.h"
#include "hal_i2c.h"
#include "../heater.h"
#include "../heat_ctrl.h"

#if ENABLE_CLI == 0
void cli_init(void){}
void cli_printargs(char *argv[], uint8_t argc){}
#else

#define UNKNOWN_CMD         "Unknown command. Type ? for help"
#define NBRIEF              81
#define NMSGBUFFER          500
//1: some flash memory tests can be controlled using buttons on devkit
#define ENABLE_MEM_TESTS    0

//############################# domains definitions ###########################
//DM_xx constants must reflect the index inside domains array
#define DM_XGA          0
#define DM_MX25R64      1       //SPI flash memory
#define DM_MAX30001     2       //MAX30001 driver
#define STD_DOMAIN      DM_XGA  //default domain at start-up

typedef struct
{
    uint8_t id;
    const char *name;
    const char *prompt;
}str_domain;

//when changing the domains array, dont forget to update 'set_domain' function
static const str_domain domains[] =
{
    {DM_XGA,            "xga",      "\r\nxga> "},
    {DM_MX25R64,        "mx25r64",  "\r\nmem> "},
	{DM_MAX30001,       "max",      "\r\nmax> "},
};
static const uint16_t NDOMAINS = sizeof(domains)/sizeof(str_domain);

//############################ function prototypes ############################
static void tsk_cli_update(void *params);
static CliCommand *get_command(char *name,const CliCommand *array,uint16_t len);
static void set_domain(uint8_t domain);
static bool is_reserved_cmd(char *name);
static int16_t is_valid_domain(char *name);
static bool arg2uint32(char *arg, uint32_t *dest, int base) __attribute__((unused));

//############################ commands prototypes ############################
static void cmd_help(char *argv[], uint8_t argc);
static void cmd_cd_domain(char *argv[], uint8_t argc);
static void cmd_ls_domain(char *argv[], uint8_t argc);
static void cmd_dump_stack(char *argv[], uint8_t argc);
static void cmd_info(char *argv[], uint8_t argc);
static void cmd_top(char *argv[], uint8_t argc);
static void cmd_inc_error(char *argv[], uint8_t argc);
static void cmd_generate_hf(char *argv[], uint8_t argc);
static void cmd_info_heap(char *argv[], uint8_t argc);
static void cmd_unleash_wdt(char *argv[], uint8_t argc);
static void cmd_set_period(char *argv[], uint8_t argc);
static void cmd_set_vcc(char *argv[], uint8_t argc);
static void cmd_set_duty(char *argv[], uint8_t argc);
static void cmd_enable_sys_print(char *argv[], uint8_t argc);
static void cmd_list_vcc(char *argv[], uint8_t argc);
static void cmd_reset(char *argv[], uint8_t argc);
static void cmd_vheater_en(char *argv[], uint8_t argc);

//################################# variables #################################
static char brief_help[NBRIEF]; //buffer used to store brief help message
static uint8_t domain_id;       //holds the actual domain
static char *prompt;            //holds the actuam prompt string
static const CliCommand *cmdarray = NULL;
static uint16_t len_cmdarray = 0;

//############################### XGA commands ################################
//help messages
#define HELP_XGA "Shows available commands for XGA module.\r\n \
For help to a specific command, use: ? <command>\r\n"
#define HELP_CD "Change the terminal to a specific domain.\r\n \
A domain is a separate environment where specific\r\n \
functions are executed. Domains are implemented to\r\n \
provide better division to not related functions\r\n \
Use: cd <domain name>\r\n"
#define HELP_LS         "List available domains\r\n"
#define HELP_INFO       "Prints board information\r\n"
#define HELP_TOP        "Prints stack usage for each RTOS task\r\n"
#define HELP_INC_ERR    "Increments error counters\r\n"
#define HELP_GENHF      "Generates hardfault to test diagnostics\r\n"
#define HELP_INFO_HEAP  "Prints information about system heap\r\n"
#define HELP_DUMP_STACK "Dumps stack levels using BLE log messages\r\n"
#define HELP_UNLEASH_WDT "Dont feed watchdog\r\n"
#define HELP_SET_PERIOD "Set heat main period in milliseconds\r\n"
#define HELP_SET_VCC 	"Set battery VCC (value must be x10. Ex: 50 = 5.0V)\r\n"
#define HELP_SET_DUTY	"Set channel duty cycle <channel> <duty>\r\n"
#define HELP_EN_SYS_PRINT	"enable/disable heat params printing\r\n"
#define HELP_LIST_VCC	"List available battery voltages (x10)\r\n"
#define HELP_RESET		"Reset board\r\n"
#define HELP_VHEATER_EN	"Enable disable VHEATER_EN. Use: vheater <0/1>\r\n"

static const CliCommand xga_commands[] =
{
    {"?",       HELP_XGA,       cmd_help},
    {"help",    HELP_XGA,       cmd_help},
    {"cd",      HELP_CD,        cmd_cd_domain},
    {"heap",    HELP_INFO_HEAP, cmd_info_heap},
    {"info",    HELP_INFO,      cmd_info},
	{"dstack",  HELP_DUMP_STACK,cmd_dump_stack},
    {"ls",      HELP_LS,        cmd_ls_domain},
    {"top",     HELP_TOP,       cmd_top},
    {"ince",    HELP_INC_ERR,   cmd_inc_error},
    {"genhf",   HELP_GENHF,     cmd_generate_hf},
    {"wdtunl",  HELP_UNLEASH_WDT,cmd_unleash_wdt},
	{"period", 	HELP_SET_PERIOD, cmd_set_period},
	{"vcc",		HELP_SET_VCC, 	cmd_set_vcc},
	{"duty", 	HELP_SET_DUTY,	cmd_set_duty},
	{"p", 		HELP_EN_SYS_PRINT, cmd_enable_sys_print},
    {"lsvcc", 	HELP_LIST_VCC, cmd_list_vcc},
    {"reset", 	HELP_RESET,    cmd_reset},
    {"vheater", HELP_VHEATER_EN, cmd_vheater_en}
};
//commands that can only be executed on xga domain
static const char* reserved_cmds[] = {"?","help","cd","ls"};
static const uint8_t NRESERVED_CMD = sizeof(reserved_cmds)/sizeof(char*);

//number of commands
static const uint16_t NXGA_CMD = sizeof(xga_commands)/sizeof(CliCommand);
static bool configured = false;

void cli_init(void)
{
    TaskHandle_t hnd;
    if(!configured)
    {
        set_domain(STD_DOMAIN);
        shell_init();
        hnd = tsk_create(TSK_LIB_CLI, tsk_cli_update);
        ASSERT_TASK(hnd);
        configured = true;
        SEGGER_RTT_Init();
    }
}


#define IN_BUFFER           5
#define CLITASK_DELAY       20
#define ADC_TRIGGER_TIME    (1500/CLITASK_DELAY)
static void tsk_cli_update(void *params)
{
    char **argv;
    const CliCommand *cmd;
    uint8_t i, argc, inbuffer[IN_BUFFER], nreceived, *newchar;

    SEGGER_RTT_Init();
    cli_print(prompt);
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(CLITASK_DELAY));
        nreceived = SEGGER_RTT_Read(SEGGER_OUT_BUFFER, &inbuffer, sizeof(inbuffer));
        newchar = inbuffer;
        for(i = 0; i < nreceived; i++, newchar++)
        {
            cli_print((char*)shell_newChar(*newchar));
            if(shell_newCommand())
            {
                argc = shell_getArgs((int8_t***)&argv);
                if(argc)
                {
                    //only XGA domain can be used to execute reserved commands
                    if(is_reserved_cmd(argv[0]))
                        cmd = get_command(argv[0], xga_commands, NXGA_CMD);
                    else //command from actual domain
                        cmd = get_command(argv[0], cmdarray, len_cmdarray);
                    if(NULL == cmd)
                        cli_print(NEW_LINE UNKNOWN_CMD);
                    else
                        cmd->func(argv, argc);
                }
                shell_reset();
                cli_print(prompt);
            }
        }
    }
}

void cli_exec_cmd(uint8_t domain, char *argv[], uint8_t argc)
{
    CliCommand *cmd;
    uint8_t old_domain = domain_id;
    if(domain_id != domain) //domain need to be changed
        set_domain(domain);
    cmd = get_command(argv[0], cmdarray, len_cmdarray);
    if(cmd)
        cmd->func(argv, argc);
    if(old_domain != domain_id)
        set_domain(old_domain);
}

static int16_t is_valid_domain(char *name)
{
    uint16_t i;
    int16_t index = -1;
    const str_domain *dm;

    dm = domains;
    for(i = 0; i < NDOMAINS; i++, dm++)
    {
        if(strcmp(dm->name, name) == 0)
        {
            index = i;
            break;
        }
    }
    return index;
}

static bool is_reserved_cmd(char *name)
{
    uint8_t i;
    bool reserved;

    reserved = false;

    for(i = 0; i < NRESERVED_CMD; i++)
    {
        if(strcmp(reserved_cmds[i],name) == 0)
        {
            reserved = true;
            break;
        }
    }
    return reserved;
}

void cli_printargs(char *argv[], uint8_t argc)
{
    uint8_t i;
    cli_print(NEW_LINE);
    for(i = 0; i < argc; i++)
        cli_print("[%d]: %s\r\n", i, argv[i]);
}

static CliCommand *get_command(char *name, const CliCommand *array,
                               uint16_t len)
{
    uint16_t i;
    CliCommand *found;
    const CliCommand *cmd;

    if(NULL == array || 0 == len)
        return NULL;
    found = NULL;
    cmd = array;
    for(i = 0; i < len; i++, cmd++)
    {
        if(strcmp(name, cmd->name) == 0)
        {
            found = (CliCommand*)cmd;
            break;
        }
    }
    return found;
}

static void set_domain(uint8_t domain)
{
    if(DM_XGA == domain)
    {
        domain_id = DM_XGA;
        prompt = (char*)shell_getPrompt();
        cmdarray = xga_commands;
        len_cmdarray = NXGA_CMD;
    }
    else if(DM_MX25R64 == domain)
    {
        domain_id = DM_MX25R64;
        prompt = (char*)domains[domain_id].prompt;
        len_cmdarray = cli_mx2564_get_commands(&cmdarray);
    }
}

static void copy_brief_help(const char *help)
{
    uint8_t i;
    char *brief = brief_help;
    for(i = 0; i < NBRIEF; i++, help++, brief++)
    {
        *brief = *help;
        if((*brief) == '\r')
        {
            *brief = 0;
            break;
        }
    }
}

//######################### commands implementation ###########################

static void cmd_help(char *argv[], uint8_t argc)
{
    uint16_t i;
    const CliCommand *cmd;

    cmd = cmdarray;
    if(NULL == cmd || len_cmdarray == 0)
    {
        cli_print(UNKNOWN_CMD);
        return;
    }
    if(argc == 1) //just "help" or "?" was typed
    {
        cli_print(NEW_LINE);
        for(i = 0; i < len_cmdarray; i++, cmd++)
        {
            copy_brief_help(cmd->help);
            cli_print("%s\t%s\r\n", cmd->name, brief_help);
        }
    }
    else if(argc >= 2) //requested help for a specific command.
    {
        cmd = get_command(argv[1], cmdarray, len_cmdarray);
        cli_print(NEW_LINE);
        if(NULL == cmd)
            cli_print(UNKNOWN_CMD);
        else
            cli_print((char*)cmd->help);
    }
}

static void cmd_dump_stack(char *argv[], uint8_t argc)
{
    if(!ble_is_notification_on(BLEMSG_LOGGER))
    {
        cli_print("\r\nBLE log message is not enabled!");
        return;
    }
    cli_print("\r\nSending stack info over BLE...");
    diag_dump(DIAG_STACK_ALL);
}

static void cmd_cd_domain(char *argv[], uint8_t argc)
{
      int16_t i = is_valid_domain(argv[1]);
      if(i >= 0)
          set_domain(i);
}

static void cmd_ls_domain(char *argv[], uint8_t argc)
{
    uint16_t i;
    const str_domain *dm;
    cli_print(NEW_LINE);
    if(NDOMAINS == 0)
    {
        cli_print("No domains available.\r\n");
        return;
    }
    dm = domains;
    for(i = 0; i < NDOMAINS; i++, dm++)
        cli_print("%s\r\n", dm->name);
}

static void cmd_info_heap(char *argv[], uint8_t argc)
{
    uint32_t heap = os_heap_size();
    uint32_t tasks = os_heap_tsk_usage();
    uint32_t queues = os_heap_queue_usage();
    float p;

    cli_print("\r\n    \t     size (bytes)      (%%)");
    cli_print("\r\n-----------------------------------");
    cli_print("\r\nheap \t\t%d\t       %d", heap, 100);
    p = (((float)tasks)/heap);
    cli_print("\r\ntasks \t\t%d\t       %d", tasks, (uint16_t)(p*100));
    p = (((float)queues)/heap);
    cli_print("\r\nqueues\t\t%d\t       %d", queues, (uint16_t)(p*100));
}

#define LEN_BLE_ADDR    6
#define LEN_INFO_BUFFER 32
static void cmd_info(char *argv[], uint8_t argc)
{
    BLEStatus blest;
    int8_t i;
    uint16_t len;
    uint8_t buffer[LEN_INFO_BUFFER];

    cli_print("\r\nhardware version..: %d", HW_VERSION);
    cli_print("\r\nfirmware version..: %d.%d.%d",FWV_MAJOR,FWV_MINOR,FWV_PATCH);
    cli_print("\r\nble tx power......: %d dBm", BLE_TX_POWER);
    //read and print module name
    blest = hal_ble_get_name(buffer, &len);
    if(BLEST_OP_OK == blest)
    {
        buffer[LEN_INFO_BUFFER-1] = 0;
        if(len < LEN_INFO_BUFFER)
            buffer[len] = 0;
        cli_print("\r\nble name..........: %s", buffer);
    }
    else
        cli_print("\r\nble name..........: unknown");
    blest = hal_ble_get_addr(buffer, LEN_BLE_ADDR);
    if(BLEST_OP_OK == blest)
    {
        cli_print("\r\nble address.......: 0x");
        for(i = LEN_BLE_ADDR-1; i >= 0; i--)
            cli_print("%02X", buffer[i]);
    }
    else
        cli_print("\r\nble addr..........: invalid");
    //--- print platform information
    #if DEVKIT_NRF52 == 1
        cli_print("\r\nplatform..........: PCA10040");
    #else
        cli_print("\r\nplatform..........: SKIIN");
    #endif
    //--- print flags information
    cli_print("\r\nadc mode..........: %d", adc_get_mode());
}

static void cmd_top(char *argv[], uint8_t argc)
{
    uint8_t stack_use[MAX_TASKS];
    const TaskData *tsk;
    uint8_t i;

    os_update_stack_level();
    if(!os_get_stack_usage(stack_use, MAX_TASKS, false))
    {
        cli_print("\r\nError reading stack info");
        return;
    }
    cli_print("\r\ntask\t     stack (bytes)  stack used (%%)");
    cli_print("\r\n------------------------------------------");
    for(i = 0; i < MAX_TASKS; i++)
    {
        tsk = os_get_taskconfig((TaskId)i);
        if(NULL == tsk)
            continue;
        cli_print("\r\n%s\t\t %d\t\t %d",tsk->name, tsk->stack*4, stack_use[i]);
    }
}

static void cmd_inc_error(char *argv[], uint8_t argc)
{
    uint16_t f1, f2;
    f1 = diag_inc_flag(FLG_BLE_CMD_OVF);
    f2 = diag_inc_flag(FLG_BLE_CMD_OVF);
    cli_print("\r\ne_f1: %d, e_f2: %d", f1, f2);
}

static void cmd_generate_hf(char *argv[], uint8_t argc)
{
//    sys_update_timebase();
    void(*hf)(void);
    hf = NULL;
    hf();
}

static bool arg2uint32(char *arg, uint32_t *dest, int base)
{
    uint64_t temp;
    if(!str_2uint(arg, &temp, base))
    {
        cli_print("\r\nFailed to convert arg %s. Use base %d", arg, base);
        return false;
    }
    *dest = (uint32_t)temp;
    return true;
}

static void cmd_unleash_wdt(char *argv[], uint8_t argc)
{
    cli_print("\r\nStop feeding WDT. Should reset soon");
    wdt_ignore_feed();
}

static void cmd_enable_sys_print(char *argv[], uint8_t argc)
{
	heat_cycle_printing();
}

#define N_VOLTAGES	10
static void cmd_list_vcc(char *argv[], uint8_t argc)
{
	bool success;
	uint8_t nvoltages = N_VOLTAGES;
	uint8_t voltages[N_VOLTAGES] = {0};
	success = tps_get_available_voltages(voltages, &nvoltages);
	if(success)
	{
		cli_print("\r\nAvailable voltages (x10): ");
		for(uint i = 0; i < nvoltages; i++)
			cli_print("%d   ", voltages[i]);
		cli_print("\r\n");
	}
	else
		cli_print("\r\nFailed to read voltages from tps module");
}

static void cmd_set_period(char *argv[], uint8_t argc)
{
	uint32_t temp32;
	uint16_t period;
	app_status_t status;

	check_args(argc, 2);
	//get channel
	if(!arg2uint32(argv[1], &temp32, 10))
		return;
	period = (uint16_t)temp32;
	//set duty cycle
	status = hw_set_duty_period(period, false);
	if(APPST_SUCCESS != status)
	{
		cli_print("\r\n set period failed: %d", status);
	}
}

static void cmd_set_vcc(char *argv[], uint8_t argc)
{
	uint32_t temp32;
	pmic_status_t status;
	check_args(argc, 1);
	if(!arg2uint32(argv[1], &temp32, 10))
		return;
	if(temp32 > 255)
	{
		cli_print("\r\nMaximum value is 255");
		return;
	}
	uint8_t vcc = (uint8_t)temp32;
	if(!tps_voltagelevel_valid(vcc))
	{
		cli_print("\r\n %s is not supported. Use lsvcc to check valid options", argv[1]);
		return;
	}
	status = tps_neg_contract(vcc);
	if(PMICST_OK != status)
	{
		cli_print("\r\nFailed to set VCC. Code: %d", status);
	}
}

static void cmd_set_duty(char *argv[], uint8_t argc)
{
	uint32_t temp32;
	uint8_t channel, duty;
	app_status_t status;

	check_args(argc, 2);
	//get channel
	if(!arg2uint32(argv[1], &temp32, 10))
		return;
	channel = (uint8_t)temp32;
	//get duty
	if(!arg2uint32(argv[2], &temp32, 10))
		return;
	duty = (uint8_t)temp32;
	//set duty cycle
	status = hw_set_dutycycle(channel, duty);
	if(APPST_SUCCESS != status)
	{
		cli_print("\r\n set duty failed: %d", status);
	}
}

static void cmd_vheater_en(char *argv[], uint8_t argc)
{
	uint32_t temp32;
	check_args(argc, 1);
	if(!arg2uint32(argv[1], &temp32, 10))
		return;
	if(temp32 > 1)
	{
		cli_print("\r\nInvalid argument: %s", argv[1]);
		return;
	}
	if(temp32)
		hal_gpio_set(V_HEATER_EN);
	else
		hal_gpio_clr(V_HEATER_EN);
	cli_print("\r\nvheater_en set to %d", temp32);
}

static void cmd_reset(char *argv[], uint8_t argc)
{
	NVIC_SystemReset();
}

#endif //ENABLE_CLI
