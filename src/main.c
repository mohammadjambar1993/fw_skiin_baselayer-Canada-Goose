#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "adc_ctrl.h"
#include "appconfig.h"
#include "sdk_config.h"
#include "hal_config.h"
#include "hal_gpio.h"
#include "FreeRTOS.h"
#include "task.h" // Added for vTaskDelay and Task creation
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

// ---------------------------------------------------------
// FIXED AUTO-START TASK: TARGET CHANNEL B (INDEX 1)
// ---------------------------------------------------------
void auto_start_heat_task(void * pvParameters)
{
    // Wait for power bank to wake up (5 seconds is good)
    vTaskDelay(pdMS_TO_TICKS(5000)); 

    cmd_heat_params_t params;
    memset(&params, 0, sizeof(cmd_heat_params_t));

    params.timeout_secs = 43200; // 12 Hours
    params.pwm_period = hw_get_default_period_count();

    log_info(">> HEAT TASK STARTING <<");

    while(1)
    {
        // Dynamically get voltage. If it's 0, the power bank is asleep.
        params.voltage = hw_get_current_voltage();
        
        if (params.voltage == 0) {
            log_warn("Waiting for Power Bank (VCC is 0)...");
        } else {
            // Send command to heat_ctrl.c
            // Note: Index 1 is Channel B
            heat_set_channels(&params);
            log_debug("Heat Heartbeat Sent. Vcc: %d", params.voltage);
        }

        // Re-send command every 10 seconds to keep session alive
        vTaskDelay(pdMS_TO_TICKS(10000)); 
    }
}

// ... rest of the forward declarations ...
void show_reset(void);
void pins_init(void);
void config_uicr(void);

int main(void)
{
    // Hardware Peripheral Initialization
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
    
    // Power Management Initialization
    if (!pmic_init())
        log_error(">> fail to init pmic <<\r\n");
        
    util_blocking_delay_ms(10); // Increased stability delay

    sys_init();
    ble_init();
    cli_init();

    // Safety Watchdog
    wdt_init();
    wdt_start();
    
    if(!os_init())
        log_error(">> Failed to initialize OS <<\r\n");

    // ----------------------------------------------------
    // START THE HEATER TASK
    // ----------------------------------------------------
    xTaskCreate(auto_start_heat_task, "AutoHeat", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    
    // Should never reach here
    while(1)
    {
        hal_gpio_clr(LED_R);
        hal_gpio_clr(LED_G);
        hal_gpio_clr(LED_B);
    }
}

// --- PERIPHERAL HELPERS ---

#define TOGGLE_COUNTER  4
void show_reset(void)
{
    volatile uint8_t i;
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
        // Clear FPU irq flags to allow the nRF52833 to enter low-power sleep
        __set_FPSCR(__get_FPSCR()  & ~(FPU_EXCEPTION_MASK));
        (void) __get_FPSCR();
        NVIC_ClearPendingIRQ(FPU_IRQn);
        sd_app_evt_wait();
    }
}

void config_uicr(void)
{
    // Accessing NVMC to configure NFC pins as standard GPIO
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    
    if(0xFFFFFFFE != NRF_UICR->NFCPINS)
        *((uint32_t*)&NRF_UICR->NFCPINS) = 0xFFFFFFFE;
        
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
}

void vApplicationStackOverflowHook(TaskHandle_t *task, signed char *taskName) { while(1); }
void vApplicationMallocFailedHook(void) { while(1); }