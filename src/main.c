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
#include "heater.h"  // Access to hw_ functions
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

// Forward Declarations
void show_reset(void);
void pins_init(void);
void config_uicr(void);
void config_ram_ret(void);

// ---------------------------------------------------------
// SMART START TASK: Wait for BLE, then FORCE MAX HEAT
// ---------------------------------------------------------
void auto_start_heat_task(void * pvParameters)
{
    // 1. Wait for system initialization
    vTaskDelay(pdMS_TO_TICKS(1000)); 

    log_info(">> HEATER SYSTEM READY: Waiting for Phone Connection... <<");

    // 2. WAIT FOR BLUETOOTH CONNECTION
    // The task will pause here forever until a phone connects.
    while(!ble_is_connected()) 
    {
        vTaskDelay(pdMS_TO_TICKS(500)); // Check every 0.5 seconds
    }

    log_info(">> PHONE CONNECTED! STARTING MAX HEAT SEQUENCE... <<");

    // 3. Give it 2 seconds to settle after connection
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 4. PREPARE MAX POWER SETTINGS
    // We check the actual voltage available from the Power Bank
    uint16_t voltage_mv = hw_get_current_voltage();
    log_info(">> BATTERY VOLTAGE: %d mV <<", voltage_mv);

    cmd_heat_params_t auto_params;
    memset(&auto_params, 0, sizeof(cmd_heat_params_t));

    // Set duration to 12 hours (43200 seconds)
    auto_params.timeout_secs = 43200; 
    
    // Set Voltage and Period
    auto_params.voltage = voltage_mv;
    auto_params.pwm_period = hw_get_default_period_count();

    // 5. Turn ALL 4 Channels to 100% (MAX POWER)
    // Note: Ensure heat_ctrl.c is fixed to accept DUTY_HIGH
    auto_params.data[0] = 100; // Channel A
    auto_params.data[1] = 100; // Channel B
    auto_params.data[2] = 100; // Channel C
    auto_params.data[3] = 100; // Channel D

    log_info(">> COMMAND: Setting ALL Channels to 100%% <<");

    // 6. Send Command to Heater Controller
    app_status_t status = heat_set_channels(&auto_params);

    if (status == APPST_SUCCESS) {
        log_info(">> SUCCESS: Max Heat Activated via BLE Trigger <<");
    } else {
        log_error(">> FAILURE: Command Rejected with Error: %d <<", status);
    }

    // 7. Monitor Loop
    // This keeps the task alive to monitor voltage and connection status
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        uint16_t v = hw_get_current_voltage();
        log_info(">> STATUS CHECK: Voltage is %d mV <<", v);

        // Optional: Warn if phone disconnects
        if(!ble_is_connected()) {
            log_info(">> WARNING: Phone Disconnected (Heater currently stays ON) <<");
        }
    }
}
// ---------------------------------------------------------

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
    
    if (!pmic_init())  // Initialize i2c and power bank communication
        log_error(">> fail to init pmic <<\r\n");
        
    util_blocking_delay_ms(5);  // wait for stability

    sys_init();
    ble_init();
    cli_init();

    wdt_init();
    wdt_start();
    
    if(!os_init())
        log_error(">> Failed to initialize OS <<\r\n");

    // ----------------------------------------------------
    // CREATE THE AUTO-HEAT TASK
    // ----------------------------------------------------
    // This creates the task defined above that waits for BLE
    xTaskCreate(auto_start_heat_task, "AutoStart", 256, NULL, 1, NULL);
    // ----------------------------------------------------

    vTaskStartScheduler();
    
    while(1) // Should never reach here
    {
        // Turn all leds on to indicate crash/error
        hal_gpio_clr(LED_R);
        hal_gpio_clr(LED_G);
        hal_gpio_clr(LED_B);
    }
}

#define TOGGLE_COUNTER  4
void show_reset(void)
{
    volatile uint8_t i;
    // Turn all leds off (High = Off for common anode)
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
        // Clear FPU irq flags in order to let the core to sleep
        __set_FPSCR(__get_FPSCR()  & ~(FPU_EXCEPTION_MASK));
        (void) __get_FPSCR();
        NVIC_ClearPendingIRQ(FPU_IRQn);

        sd_app_evt_wait();
    }
}

void write_uicr(uint32_t *address, uint32_t value)
{
    // Turn on flash write enable and wait until the NVMC is ready
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    
    // Write memory
    *address = value;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    
    // Turn off flash write enable and wait until the NVMC is ready
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
}

void config_uicr(void)
{
#if CONFIG_UICR == 1
    if(0xFFFFFF00 != NRF_UICR->APPROTECT) // Disable debugger access protection if needed
        write_uicr((uint32_t*)&NRF_UICR->APPROTECT, 0xFFFFFF00);
    if(0xFFFFFFFE != NRF_UICR->NFCPINS) // Configure NFC pins as standard gpios
        write_uicr((uint32_t*)&NRF_UICR->NFCPINS, 0xFFFFFFFE);
#endif
    if(0xFFFFFFFE != NRF_UICR->NFCPINS) // Ensure NFC pins are GPIOs
        write_uicr((uint32_t*)&NRF_UICR->NFCPINS, 0xFFFFFFFE);
}

// Stack overflow check
void vApplicationStackOverflowHook(TaskHandle_t *task, signed char *taskName)
{
    while(1)
    {
        __NOP();
    }
}

// Malloc failed hook
void vApplicationMallocFailedHook(void)
{
    while(1)
    {
        __NOP();
    }
}