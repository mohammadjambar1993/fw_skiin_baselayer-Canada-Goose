#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "adc_ctrl.h"
#include "appconfig.h"
#include "sdk_config.h"
#include "hal_config.h"
#include "hal_gpio.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "logger.h"
#include "logio.h"
#include "mya_util.h"
#include "nrf_soc.h"
#include "ble_rpc.h"
#include "tskctrl.h"
#include "nrf_sdm.h"
#include "heat_ctrl.h"
#include "cli.h"
#include "pmic.h"
#include "wdt.h"
#include "gtk_spi.h"
#include "hal_i2c.h"
#include "crc16.h"
#include "drivers/ina231.h"
#include "diagnostic.h"
#include "nrf52833.h"
#include "nrf.h"
#include "system.h"
#include "temperature_ads.h"

void show_reset(void);
void pins_init(void);
void config_uicr(void);
void config_ram_ret(void);

int main(void)
{
  	hal_config_init();
    config_uicr();
    if(IOST_MAP_OK != hal_gpio_init())
   		log_error(">> IO MAP ERROR <<\r\n");
    pins_init();
    log_init(LEVEL_DEBUG, LOG_SEGGER_RTT);
    logio_init();
    if(!gtk_spi_init())
    	log_error(">> fail to init SPI GTK <<\r\n");
    show_reset();
    diag_init();
	if (!pmic_init())  //initialize i2c and power bank
		log_error(">> fail to init pmic <<\r\n");
	util_blocking_delay_ms(5);  //wait for 5ms so that it is stable

	sys_init();
	ble_init();
    cli_init();

    wdt_init();
    wdt_start();
   	if(!os_init())
        log_error(">> Failed to initialize OS <<\r\n");
    vTaskStartScheduler();
    while(1) //should never reach here
    {
        //turn all leds on
        hal_gpio_clr(LED_R);
        hal_gpio_clr(LED_G);
        hal_gpio_clr(LED_B);
    }
}

#define TOGGLE_COUNTER  4
void show_reset(void)
{
    volatile uint8_t i;
    //turn all leds off
    hal_gpio_set(LED_R);
    hal_gpio_set(LED_G);
    hal_gpio_set(LED_B);

    for(i = 0; i < TOGGLE_COUNTER; i++)
    {
        hal_gpio_toggle(LED_R);
        hal_gpio_toggle(LED_G);
        hal_gpio_toggle(LED_B);
        util_blocking_delay_ms(100);
    }
    log_info(">>RESET<<");
}

void pins_init(void)
{
    hal_gpio_set(CS_IMU);
    hal_gpio_set(CS_MEM);
}

#define FPU_EXCEPTION_MASK  0x0000009F
void vApplicationIdleHook(void)
{
    while(1)
    {
        //clear FPU irq flags in order to let the core to sleep
        __set_FPSCR(__get_FPSCR()  & ~(FPU_EXCEPTION_MASK));
        (void) __get_FPSCR();
        NVIC_ClearPendingIRQ(FPU_IRQn);

        sd_app_evt_wait();
    }
}

void write_uicr(uint32_t *address, uint32_t value)
{
    //turn on flash write enable and wait until the NVMC is ready
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    //write memory
    *address = value;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    //turn off flash write enable and wait until the NVMC is ready
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
}

void config_uicr(void)
{
#if CONFIG_UICR == 1
    if(0xFFFFFF00 != NRF_UICR->APPROTECT) //disable debugger access
        write_uicr((uint32_t*)&NRF_UICR->APPROTECT, 0xFFFFFF00);
    if(0xFFFFFFFE != NRF_UICR->NFCPINS) //configure NFC pins as standard gpios
        write_uicr((uint32_t*)&NRF_UICR->NFCPINS, 0xFFFFFFFE);
#endif
    if(0xFFFFFFFE != NRF_UICR->NFCPINS) //configure NFC pins as standard gpios
        write_uicr((uint32_t*)&NRF_UICR->NFCPINS, 0xFFFFFFFE);
}

//stack overflow check must not be enabled on production code!
void vApplicationStackOverflowHook(TaskHandle_t *task, signed char *taskName)
{
    while(1)
    {
        __NOP();
    }
}

void vApplicationMallocFailedHook(void)
{
    while(1)
    {
        __NOP();
    }
}

