/*
 * hal_spi_nrf52.c
 *
 *  Created on: Aug 25, 2016
 *      Author: Myant
 */
#include "hal_spi.h"
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833) && (ENABLE_HAL_SPI == 1)

#include "nrf_drv_spi.h"
#include "nrf_drv_common.h"
#include "app_util_platform.h"
#include "logger.h"
#include "nrf_gpio.h"


typedef void(*irq_spi_t)(nrf_drv_spi_evt_t const * p_event, void *context);

typedef struct
{
    ret_code_t initialized;
    bool enabled;
    spi_tx_callback_t callback;
    irq_spi_t irq_spi;
    nrf_drv_spi_t nrf_spi;
    nrf_drv_spi_config_t nrf_config;
}spi_ctrl_t;

static void irq_spi0(nrf_drv_spi_evt_t const * p_event, void *context);
static void irq_spi1(nrf_drv_spi_evt_t const * p_event, void *context);

static spi_ctrl_t port_map[] =
{
    //SPI1 (0)
    {
        .initialized = false,
        .enabled = false,
        .callback = NULL,
        .irq_spi = irq_spi0,
        .nrf_spi =
        {
            .inst_idx = NRFX_SPIM0_INST_IDX,
            .u.spim.p_reg = NRF_SPIM0,
            .u.spim.drv_inst_idx = NRFX_SPIM0_INST_IDX,
            .use_easy_dma = SPI0_USE_EASY_DMA
        },
        .nrf_config =
        {
        #if DEVKIT_NRF52 == 1
        	.sck_pin = 0,
            .mosi_pin = 0,
            .miso_pin = 0,
        #elif PCB_ID == PCB_MULTI_CHANNEL
			.sck_pin = NRF_GPIO_PIN_MAP(0,27),
			.mosi_pin = NRF_GPIO_PIN_MAP(0,26),
			.miso_pin = NRF_GPIO_PIN_MAP(0,4),
		#elif PCB_ID == PCB_DUAL_CHANNEL
			.sck_pin = NRF_GPIO_PIN_MAP(1,8),
			.mosi_pin = NRF_GPIO_PIN_MAP(0,17),
			.miso_pin = NRF_GPIO_PIN_MAP(0,14),
        #endif
            .ss_pin = NRF_DRV_SPI_PIN_NOT_USED,
            .irq_priority = APP_IRQ_PRIORITY_LOW,
            .orc = 0xFF,
            .frequency = NRF_DRV_SPI_FREQ_8M,
            .mode = NRF_DRV_SPI_MODE_0,
            .bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
        }
    },
    //SPI2 (1)
    {
        .initialized = false,
        .enabled = false,
        .callback = NULL,
        .irq_spi = irq_spi1,
        .nrf_spi =
        {
            .inst_idx = NRFX_SPIM2_INST_IDX,
			.u.spim.p_reg = NRF_SPIM2,
            .u.spim.drv_inst_idx = NRFX_SPIM2_INST_IDX,
            .use_easy_dma = SPI2_USE_EASY_DMA
        },
        .nrf_config =
        {
        #if DEVKIT_NRF52 == 1
            .sck_pin = NRF_GPIO_PIN_MAP(0,27),
            .mosi_pin = NRF_GPIO_PIN_MAP(0,26),
            .miso_pin = NRF_GPIO_PIN_MAP(0,2),
        #elif PCB_ID == PCB_MULTI_CHANNEL
            .sck_pin = NRF_GPIO_PIN_MAP(1,9),
            .mosi_pin = NRF_GPIO_PIN_MAP(0,14),
            .miso_pin = NRF_GPIO_PIN_MAP(0,12),
		#elif PCB_ID == PCB_DUAL_CHANNEL
            .sck_pin = NRF_GPIO_PIN_MAP(0,6),
            .mosi_pin = NRF_GPIO_PIN_MAP(1,9),
            .miso_pin = NRF_GPIO_PIN_MAP(0,13),
        #endif
            .ss_pin = NRF_DRV_SPI_PIN_NOT_USED,
            .irq_priority = APP_IRQ_PRIORITY_LOW,
            .orc = 0xFF,
            .frequency = NRF_DRV_SPI_FREQ_8M,
            .mode = NRF_DRV_SPI_MODE_1,   //for ads1220
            .bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
        }
    }
};

static inline spi_ctrl_t *getport(SPIId_e id)
{
    if(id < MAX_SPI_PORTS)
        return &port_map[id];
    return NULL;
}

spi_port_t hal_spi_init(SPIId_e id)
{
    spi_ctrl_t *port = getport(id);
    if(port && !port->initialized)
    {
        port->initialized = nrf_drv_spi_init(&port->nrf_spi, &port->nrf_config,
                                             port->irq_spi, NULL);
        port->callback = NULL;
    }
    if(NRF_SUCCESS == port->initialized)
        return port;
    return NULL;
}

void hal_spi_deinit(SPIId_e id)
{
	spi_ctrl_t *port = getport(id);
	if(port)
	{
		nrf_drv_spi_uninit(&port->nrf_spi);
		port->initialized = false;
		port->enabled = false;
		port->callback = NULL;
	}
}

bool hal_spi_is_enabled(spi_port_t spi)
{
    spi_ctrl_t *port = (spi_ctrl_t*)spi;
    if(NULL == port)
        return false;
    return port->enabled;
}

void hal_spi_enable(spi_port_t spi, bool en)
{
    spi_ctrl_t *port = (spi_ctrl_t*)spi;
    if(NULL == port)
        return;
    if(en && (NRF_SUCCESS == port->initialized))
    {
        nrf_spim_enable(port->nrf_spi.u.spim.p_reg);
        port->enabled = true;
    }
    else
    {
        nrf_spim_disable(port->nrf_spi.u.spim.p_reg);
        port->enabled = false;
    }
}

void hal_spi_enable_irq_tx(spi_port_t spi, bool en)
{
    spi_ctrl_t *port = (spi_ctrl_t*)spi;
    if(NULL == port)
        return;
    if(en && (NRF_SUCCESS == port->initialized))
    {
        //NVIC_SetPriority(port->nrf_spi.irq, port->nrf_config.irq_priority);
        //NVIC_ClearPendingIRQ(port->nrf_spi.irq);
        //NVIC_EnableIRQ(port->nrf_spi.irq);
    }
    else
    {
    	//NVIC_DisableIRQ(port->nrf_spi.irq);
    }
}

SPIStatus hal_spi_tx(spi_port_t spi, uint8_t *txdata, uint8_t *rxdata,
                     uint16_t len)
{
    ret_code_t ret;
    spi_ctrl_t *port = (spi_ctrl_t*)spi;
    if(NULL == port)
        return SPIST_INVALID_PORT;
    if(NRF_SUCCESS != port->initialized)
        return SPIST_OP_FAIL;
#if UC_ID == UC_NRF52832
    uint8_t rxlen = (len == 1) ? 0 : len; //PAN 58 fix
    ret = nrf_drv_spi_transfer(&port->nrf_spi, txdata, len, rxdata, rxlen);
#elif UC_ID == UC_NRF52833
    ret = nrf_drv_spi_transfer(&port->nrf_spi, txdata, len, rxdata, len);
#endif
    if(NRF_SUCCESS == ret)
        return SPIST_OP_OK;
    if(NRF_ERROR_BUSY == ret)
        return SPIST_BUSY;
    return SPIST_OP_FAIL;

}

SPIStatus hal_spi_set_tx_callback(spi_port_t spi, spi_tx_callback_t callb)
{
    spi_ctrl_t *port = (spi_ctrl_t*)spi;
    if(NULL == port)
        return SPIST_INVALID_PORT;
    if(NULL == callb)
        return SPIST_CALLB_INVALID;
    port->callback = callb;
    return SPIST_OP_OK;
}

uint32_t hal_spi_len_tx_buffer(spi_port_t spi)
{
    uint32_t nbytes = 0;
    spi_ctrl_t *port = (spi_ctrl_t*)spi;
    if(NULL == port)
        return nbytes;
    nbytes = port->nrf_spi.use_easy_dma ? 255 : 1;
    return nbytes;
}

static void irq_spi0(nrf_drv_spi_evt_t const * p_event, void *context)
{
    if(NRF_DRV_SPI_EVENT_DONE == p_event->type)
    {
        if(port_map[0].callback)
            port_map[0].callback();
    }
}

static void irq_spi1(nrf_drv_spi_evt_t const * p_event, void *context)
{
    if(NRF_DRV_SPI_EVENT_DONE == p_event->type)
    {
        if(port_map[1].callback)
            port_map[1].callback();
    }
}


#endif //UC_ID == UC_NRF52832
