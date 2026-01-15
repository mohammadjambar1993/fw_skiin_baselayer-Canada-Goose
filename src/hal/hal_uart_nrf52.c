#include "hal_uart.h"
#include "hal_config.h"
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833) && (ENABLE_HAL_UART == 1)

#include "nrf_uarte.h"
#include "nrf_drv_common.h"
#include "logger.h"

#define LEN_RX  1

static ret_code_t initialized = NRF_ERROR_INTERNAL;
static nrf_uart_baudrate_t baudrate = 0;

static uint8_t rx_buffer[LEN_RX];
static bool init(const nrf_drv_uart_config_t *cfg);
static void irq_uarte(nrf_drv_uart_event_t * p_event, void * p_context);
static void (*callback)(UartEvent evt, uint8_t *d, uint16_t len);
static void clear_all_irq_flags(void);

static nrf_drv_uart_t uart = NRF_DRV_UART_INSTANCE(0);	//uart instance

static const nrf_drv_uart_config_t default_config =
{
#if DEVKIT_NRF52 == 1
    .pseltxd = 28,
    .pselrxd = 29,
#else
	.pseltxd = 31,
	.pselrxd = 26,
#endif
    .pselcts = NRF_UART_PSEL_DISCONNECTED,
    .pselrts = NRF_UART_PSEL_DISCONNECTED,
    .p_context = NULL,
    .hwfc = NRF_UART_HWFC_DISABLED,
    .parity = NRF_UART_PARITY_EXCLUDED,
    .baudrate = NRF_UARTE_BAUDRATE_115200,
    .interrupt_priority = APP_IRQ_PRIORITY_LOW,
    .use_easy_dma = true
};

static bool init(const nrf_drv_uart_config_t *cfg)
{
    hal_uart_enable_globalIRQ(false);
    nrf_drv_uart_uninit(&uart);
    initialized = nrf_drv_uart_init(&uart, cfg, irq_uarte);
    if(NRF_SUCCESS == initialized)
        baudrate = cfg->baudrate;
    return (NRF_SUCCESS == initialized);
}

UartStatus hal_uart_init(void)
{
    if(NRF_SUCCESS == initialized)
        return UARTST_OP_OK;
    callback = NULL;
    if(init(&default_config))
        return UARTST_OP_OK;
    return UARTST_OP_FAIL;
}

UartStatus hal_uart_init_extern(nrf_drv_uart_config_t *config)
{
    if(NRF_SUCCESS == initialized)
        return UARTST_OP_FAIL;
    callback = NULL;
    if(init(config))
        return UARTST_OP_OK;
    return UARTST_OP_FAIL;
}

const nrf_drv_uart_config_t *hal_uart_get_config(void)
{
    return &default_config;
}

void hal_uart_deinit(void)
{
    initialized = NRF_ERROR_INTERNAL;
    callback = NULL;

    nrf_uarte_enable(NRF_UARTE0);
    nrf_uarte_int_disable(NRF_UARTE0, NRF_UARTE_INT_ENDRX_MASK |
                                      NRF_UARTE_INT_ENDTX_MASK |
                                      NRF_UARTE_INT_ERROR_MASK |
                                      NRF_UARTE_INT_RXTO_MASK);
    hal_uart_enable_globalIRQ(false);
    hal_uart_enableIRQ_RX(false);
    hal_uart_enableIRQ_TX(false);
    hal_uart_enable(false);
    clear_all_irq_flags();
    nrf_drv_uart_uninit(&uart);
}

uint32_t hal_uart_get_baudrate(void)
{
    uint32_t baudint = 0;
    nrf_uart_baudrate_t baud = baudrate;
    if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_1200)
        baudint = 1200;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_2400)
        baudint = 2400;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_4800)
        baudint = 4800;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_9600)
        baudint = 9600;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_14400)
        baudint = 14400;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_19200)
        baudint = 19200;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_28800)
        baudint = 28800;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_38400)
        baudint = 38400;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_57600)
        baudint = 57600;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_76800)
        baudint = 76800;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_115200)
        baudint = 115200;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_230400)
        baudint = 230400;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_250000)
        baudint = 250000;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_460800)
        baudint = 460800;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_921600)
        baudint = 921600;
    else if(baud == (nrf_uart_baudrate_t)NRF_UARTE_BAUDRATE_1000000)
        baudint = 1000000;

    return baudint;
}

void hal_uart_enable(bool en)
{
    if(en)
    {
        nrf_uarte_enable(NRF_UARTE0);
        nrf_drv_uart_rx(&uart, rx_buffer, LEN_RX);
    }
    else
    {
        nrf_drv_uart_rx_abort(&uart);
        nrf_drv_uart_tx_abort(&uart);
        nrf_drv_uart_rx_disable(&uart);
        nrf_uarte_disable(NRF_UARTE0);
    }
}

static inline void clear_all_irq_flags(void)
{
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_ENDRX);
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_ENDTX);
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_ERROR);
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_RXTO);
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_CTS);
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_NCTS);
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_RXSTARTED);
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_TXSTARTED);
    nrf_uarte_event_clear(NRF_UARTE0, NRF_UARTE_EVENT_TXSTOPPED);
}

void hal_uart_enableIRQ_RX(bool en)
{
    clear_all_irq_flags();
    if(en)
    {
        nrf_uarte_int_enable(NRF_UARTE0, NRF_UARTE_INT_ENDRX_MASK);
    }
    else
    {
        nrf_uarte_int_disable(NRF_UARTE0, NRF_UARTE_INT_ENDRX_MASK);
    }
}

void hal_uart_enableIRQ_TX(bool en)
{
    clear_all_irq_flags();
    if(en)
    {
        nrf_uarte_int_enable(NRF_UARTE0, NRF_UARTE_INT_ENDTX_MASK);
    }
    else
    {
        nrf_uarte_int_disable(NRF_UARTE0, NRF_UARTE_INT_ENDTX_MASK);
    }
}

void hal_uart_enable_globalIRQ(bool en)
{
    if(en)
    {
//        nrf_drv_common_irq_enable(UART0_IRQn, default_config.interrupt_priority);
		NVIC_SetPriority(UART0_IRQn, default_config.interrupt_priority);
		NVIC_ClearPendingIRQ(UART0_IRQn);
		NVIC_EnableIRQ(UART0_IRQn);
    }
    else
    {
//        nrf_drv_common_irq_disable(UART0_IRQn);
    	NVIC_DisableIRQ(UART0_IRQn);
    }
}

void hal_uart_flush_rx(void)
{
    nrf_uarte_task_trigger(NRF_UARTE0, NRF_UARTE_TASK_STOPRX);
}

void hal_uart_tx(uint8_t *data, uint16_t len)
{
    nrf_drv_uart_tx(&uart, data, len);
}

void hal_uart_tx_blocking(uint8_t *data, uint16_t len)
{
    hal_uart_tx(data, len);
    while(nrf_drv_uart_tx_in_progress(&uart));
}

uint32_t hal_uart_len_tx_buffer(void)
{
    uint32_t nbytes = 1;
    if(default_config.use_easy_dma)
        nbytes = 255;
    return nbytes;
}

UartStatus hal_uart_addCallback(uart_callb_t rxcall)
{
    if(NULL == rxcall)
        return UARTST_CALLB_INVALID;
    callback = rxcall;

    return UARTST_OP_OK;
}

static void irq_uarte(nrf_drv_uart_event_t * p_event, void * p_context)
{
    if(NULL == callback)
        return;
    if(NRF_DRV_UART_EVT_RX_DONE == p_event->type)
    {
        nrf_drv_uart_rx(&uart, rx_buffer, LEN_RX); //read uart into local buffer
        callback(UARTEVT_RXEND, rx_buffer, LEN_RX);
    }
    else if(NRF_DRV_UART_EVT_TX_DONE == p_event->type)
    {
        callback(UARTEVT_TXEND, NULL, 0);
    }
    else
    {
        nrf_uarte_int_disable(NRF_UARTE0, NRF_UARTE_INT_ENDRX_MASK |
                NRF_UARTE_INT_ENDTX_MASK |
                NRF_UARTE_INT_ERROR_MASK |
                NRF_UARTE_INT_RXTO_MASK);
        clear_all_irq_flags();
        hal_uart_enable(false);
        callback(UARTEVT_ERROR, NULL, 0);
    }
}
#endif //UC_ID == UC_NRF52832
