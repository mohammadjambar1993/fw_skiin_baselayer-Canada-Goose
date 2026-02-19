#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "appconfig.h"
#include "hal_config.h"
#include "hal_gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "logger.h"
#include "logio.h"
#include "nrf_soc.h"
#include "heat_ctrl.h"
#include "pmic.h"
#include "gtk_spi.h"
#include "system.h"
#include "temperature_ads.h"

// ---------------------------------------------------------
// AUTO-START TASK: 2-BY-2 ALTERNATING AT 90% POWER
// ---------------------------------------------------------
void auto_start_task(void * pvParameters)
{
    // Wait 5 seconds for power bank to fully wake up
    vTaskDelay(pdMS_TO_TICKS(5000)); 

    cmd_heat_params_t prm;
    memset(&prm, 0, sizeof(cmd_heat_params_t));

    prm.timeout_secs = 43200; // Run for 12 Hours
    prm.pwm_period = 1000;
    prm.voltage = 15; // Set to your input voltage

    while(1)
    {
        // --- PHASE 1: A and C at 90% (B and D OFF) ---
        prm.data[0] = 90; // A: 90%
        prm.data[1] = 0;  // B: OFF
        prm.data[2] = 90; // C: 90%
        prm.data[3] = 0;  // D: OFF
        prm.data[4] = 0;  // E: OFF

        heat_set_channels(&prm);
        
        // Wait 20 seconds before switching
        vTaskDelay(pdMS_TO_TICKS(20000)); 


        // --- PHASE 2: B and D at 90% (A and C OFF) ---
        prm.data[0] = 0;  // A: OFF
        prm.data[1] = 90; // B: 90%
        prm.data[2] = 0;  // C: OFF
        prm.data[3] = 90; // D: 90%
        prm.data[4] = 0;  // E: OFF

        heat_set_channels(&prm);
        
        // Wait 20 seconds before switching back
        vTaskDelay(pdMS_TO_TICKS(20000)); 
    }
}

// ... Boilerplate Initialization ...
void show_reset(void);
void pins_init(void);
void config_uicr(void);

int main(void)
{
    hal_config_init(); config_uicr();
    if(IOST_MAP_OK != hal_gpio_init()) while(1);
    pins_init(); log_init(LEVEL_DEBUG, LOG_SEGGER_RTT); logio_init(); 
    gtk_spi_init(); diag_init(); pmic_init();
    sys_init(); ble_init(); cli_init(); wdt_init(); wdt_start();
    if(!os_init()) log_error(">> Failed to initialize OS <<\r\n");

    // Init Heater Module
    heat_init();

    // Start Task
    xTaskCreate(auto_start_task, "Auto", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    while(1) {
        hal_gpio_clr(LED_R); hal_gpio_clr(LED_G); hal_gpio_clr(LED_B);
    }
}

void show_reset(void) {
    volatile uint8_t i;
    hal_gpio_set(LED_R); hal_gpio_set(LED_G); hal_gpio_set(LED_B);
    for(i = 0; i < 4; i++) {
        hal_gpio_toggle(LED_R); hal_gpio_toggle(LED_G); hal_gpio_toggle(LED_B);
        util_blocking_delay_ms(100);
    }
}
void pins_init(void) { hal_gpio_set(CS_IMU); hal_gpio_set(CS_MEM); }
void vApplicationIdleHook(void) { 
    __set_FPSCR(__get_FPSCR() & ~(0x0000009F)); (void) __get_FPSCR(); 
    NVIC_ClearPendingIRQ(FPU_IRQn); sd_app_evt_wait(); 
}
void config_uicr(void) { 
    if(0xFFFFFFFE != NRF_UICR->NFCPINS) {
        NRF_NVMC->CONFIG = 1; while(NRF_NVMC->READY == 0);
        *((uint32_t*)&NRF_UICR->NFCPINS) = 0xFFFFFFFE;
        while(NRF_NVMC->READY == 0); NRF_NVMC->CONFIG = 0;
    }
}
void vApplicationStackOverflowHook(TaskHandle_t *t, signed char *n) { while(1); }
void vApplicationMallocFailedHook(void) { while(1); }