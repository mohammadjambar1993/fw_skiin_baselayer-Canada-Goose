/*
 * hal_i2c_nrf52.c
 *
 *  Created on: Aug 30, 2016
 *      Author: Myant
 */

#include "hal_i2c.h"
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833) && (ENABLE_HAL_I2C == 1)
#include "nrf_drv_twi.h"
#include "nrf_twim.h"
#include "nrf_drv_common.h"
#include "app_util_platform.h"
#include "nrf_gpio.h"
#include "../appconfig.h"

#if DEVKIT_NRF52 == 1
static const nrf_drv_twi_config_t config =
{
    .scl = 0,
    .sda = 0,
    .frequency = NRF_TWIM_FREQ_400K,
    .interrupt_priority = APP_IRQ_PRIORITY_LOW
};
#elif PCB_ID == PCB_MULTI_CHANNEL
static const nrf_drv_twi_config_t config =
{
    .scl = NRF_GPIO_PIN_MAP(0,15),
    .sda = NRF_GPIO_PIN_MAP(0,17),
    .frequency = NRF_TWIM_FREQ_400K,
    .interrupt_priority = APP_IRQ_PRIORITY_LOW
};
#elif PCB_ID == PCB_DUAL_CHANNEL
static const nrf_drv_twi_config_t config =
{
    .scl = NRF_GPIO_PIN_MAP(0,26),
    .sda = NRF_GPIO_PIN_MAP(0,27),
    .frequency = NRF_TWIM_FREQ_400K,
    .interrupt_priority = APP_IRQ_PRIORITY_LOW
};
#endif //DEVKIT_NRF52

static const nrf_drv_twi_t i2c =
{
    .inst_idx = 0,
    .u.twim.p_twim = NRF_TWIM1,
    .u.twim.drv_inst_idx = NRFX_TWIM1_INST_IDX,
    .use_easy_dma = TWI1_USE_EASY_DMA
};

static void irq_i2c(nrf_drv_twi_evt_t const *evt, void *context);
static void(*irqcallback)(I2CStatus st) = NULL;
static ret_code_t initialized = NRF_ERROR_NULL;
static bool enabled = false;

I2CStatus hal_i2c_init(void)
{
    ret_code_t ret;
    //i2c is alread initialized
    if(NRF_SUCCESS == initialized)
        return I2CST_OP_OK;
    irqcallback = NULL;
    ret = nrf_drv_twi_init(&i2c, &config, irq_i2c, NULL);
    initialized = ret;
    enabled = false;
    if(NRF_SUCCESS == ret)
        return I2CST_OP_OK;
    return I2CST_OP_FAIL;
}

void hal_i2c_enable(bool en)
{
    if(en)
        nrf_drv_twi_enable(&i2c);
    else
        nrf_drv_twi_disable(&i2c);
    enabled = en;
}

void hal_i2c_enableIRQ(bool en)
{
    if(en)
    {
        NVIC_SetPriority(nrf_drv_get_IRQn((void *)i2c.u.twim.p_twim), config.interrupt_priority);
        NVIC_ClearPendingIRQ(nrf_drv_get_IRQn((void *)i2c.u.twim.p_twim));
        NVIC_EnableIRQ(nrf_drv_get_IRQn((void *)i2c.u.twim.p_twim));
    }
    else
    {
        NVIC_DisableIRQ(nrf_drv_get_IRQn((void *)i2c.u.twim.p_twim));
    }
}

I2CStatus hal_i2c_tx(uint8_t addr, uint8_t *txdata, uint16_t len, bool stop)
{
    ret_code_t ret;
    if(!enabled)
        return I2CST_OP_FAIL;
    ret = nrf_drv_twi_tx(&i2c, addr, txdata, len, !stop);
    if(NRF_SUCCESS == ret)
        return I2CST_OP_OK;
    else if(NRF_ERROR_BUSY == ret)
        return I2CST_BUSY;
    return I2CST_OP_FAIL;
}

I2CStatus hal_i2c_rx(uint8_t addr, uint8_t *rxdata, uint16_t len)
{
    ret_code_t ret;
    if(!enabled)
        return I2CST_OP_FAIL;
    ret = nrf_drv_twi_rx(&i2c, addr, rxdata, len);
    if(NRF_SUCCESS == ret)
        return I2CST_OP_OK;
    else if(NRF_ERROR_BUSY == ret)
        return I2CST_BUSY;
    return I2CST_OP_FAIL;
}

I2CStatus hal_i2c_set_irq_callback(void(*callb)(I2CStatus st))
{
    if(NULL == callb)
        return I2CST_CALLB_INVALID;
    irqcallback = callb;
    return I2CST_OP_OK;
}

uint32_t hal_i2c_len_tx_buffer(void)
{
    uint32_t nbytes = 1;
    if(i2c.use_easy_dma)
        nbytes = 255;
    return nbytes;
}

uint32_t hal_i2c_len_rx_buffer(void)
{
    uint32_t nbytes = 1;
    if(i2c.use_easy_dma)
        nbytes = 255;
    return nbytes;
}

static void irq_i2c(nrf_drv_twi_evt_t const *evt, void *context)
{
    if(NULL == irqcallback)
        return;
    if(NRF_DRV_TWI_EVT_DONE == evt->type)
    {
        irqcallback(I2CST_IRQ_OP_DONE);
    }
    else if(NRF_DRV_TWI_EVT_ADDRESS_NACK == evt->type)
    {
        irqcallback(I2CST_IRQ_ADDR_NACK);
    }
    else if(NRF_DRV_TWI_EVT_DATA_NACK == evt->type)
    {
        irqcallback(I2CST_IRQ_DATA_NACK);
    }
}

#endif //UC_ID == UC_NRF52832
