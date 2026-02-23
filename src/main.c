#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "appconfig.h"
#include "hal_config.h"
#include "hal_gpio.h"
#include "FreeRTOS.h"
#include "logger.h"
#include "logio.h"
#include "mya_util.h"
#include "heat_ctrl.h"
#include "pmic.h"
#include "wdt.h"
#include "gtk_spi.h"
#include "system.h"
#include "nrf_sdm.h"

/* FORWARD DECLARATIONS */
static void show_reset(void);
static void pins_init(void);
static void config_uicr(void);
static void start_brute_force_heating(void);

int main(void)
{
    hal_config_init();
    config_uicr();

    if(IOST_MAP_OK != hal_gpio_init())
        log_error(">> IO MAP ERROR <<\r\n");

    pins_init();
    log_init(LEVEL_DEBUG, LOG_SEGGER_RTT);
    logio_init();

    if(!gtk_spi_init()) log_error(">> fail SPI <<\r\n");

    show_reset();
    diag_init();
    pmic_init();
    util_blocking_delay_ms(5);
    sys_init();

    /* NO BLE INIT */
    // ble_init(); 

    cli_init();
    wdt_init();
    wdt_start();

    if(!os_init()) log_error(">> Failed OS <<\r\n");

    /* HEAT CONTROLLER INIT */
    if(APPST_SUCCESS != heat_init())
        log_error(">> Failed heat init <<\r\n");

    /* START IMMEDIATELY */
    start_brute_force_heating();

    vTaskStartScheduler();

    while(1) { }
}

static void start_brute_force_heating(void)
{
    // The params don't matter because heat_ctrl.c is hardcoded 
    // to Channel B / 45C / Max Power.
    cmd_heat_params_t params = {0};
    
    // Just trigger the session
    heat_set_channels(&params); 
    
    log_info(">>> BRUTE FORCE HEATING STARTED <<<");
}

/* ============================================================= */
/* BOILERPLATE                              */
/* ============================================================= */

#define TOGGLE_COUNTER 4
static void show_reset(void)
{
    hal_gpio_set(LED_R); hal_gpio_set(LED_G); hal_gpio_set(LED_B);
    for(uint8_t i = 0; i < TOGGLE_COUNTER; i++)
    {
        hal_gpio_toggle(LED_R); hal_gpio_toggle(LED_G); hal_gpio_toggle(LED_B);
        util_blocking_delay_ms(100);
    }
    log_info(">> RESET <<");
}

static void pins_init(void)
{
    hal_gpio_set(CS_IMU);
    hal_gpio_set(CS_MEM);
}

void vApplicationIdleHook(void)
{
    while(1) { sd_app_evt_wait(); }
}

static void config_uicr(void)
{
    if(0xFFFFFFFE != NRF_UICR->NFCPINS)
    {
        NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
        NRF_UICR->NFCPINS = 0xFFFFFFFE;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
        NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    }
}

void vApplicationStackOverflowHook(TaskHandle_t *task, signed char *taskName) { while(1); }
void vApplicationMallocFailedHook(void) { while(1); }