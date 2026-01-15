#include "hal_config.h"
#if (UC_ID == UC_NRF52832 || UC_ID == UC_NRF52833)

#include "hal_gpio.h"
#include "nrf_gpio.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_common.h"
#include "logger.h"

//P0.00 will be ignored and should be used when a pin is not present
#define UNUSED_PIN  0

typedef enum
{
    INPUT,
    OUTPUT
}direction;

typedef struct
{
    uint32_t pin;
    direction dir;
    nrf_gpio_pin_pull_t pull;
}io_pin;

typedef struct
{
    uint32_t pin;
    nrf_gpio_pin_pull_t pull;
    nrf_gpiote_polarity_t edge;
    pinirq_callb_t callback;
}pin_irq;

#if DEVKIT_NRF52 == 1 //running code using PCA10040 development kit
#define  PIN_IMU_IRQ UNUSED_PIN
static const io_pin iomap[] =
{
    //outputs
    {NRF_GPIO_PIN_MAP(0,14),OUTPUT,NRF_GPIO_PIN_NOPULL},//LED_R     - LED_1
    {NRF_GPIO_PIN_MAP(0,15),OUTPUT,NRF_GPIO_PIN_NOPULL},//LED_G     - LED_2
    {NRF_GPIO_PIN_MAP(0,16),OUTPUT,NRF_GPIO_PIN_NOPULL},//LED_B     - LED_3
	{UNUSED_PIN, OUTPUT,NRF_GPIO_PIN_PULLUP},    //MEM_RST
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLUP},    //MEM_CS
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLUP},    //IMU_CS
	{UNUSED_PIN,INPUT,NRF_GPIO_PIN_PULLUP},              //IMU_INT

	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //HEATING_ChA
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //HEATING_ChB
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //HEATING_ChC
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //HEATING_ChD
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //HEATING_ChE
	{UNUSED_PIN, OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //PD_RESET

	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //TEMP_A
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //TEMP_B
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //TEMP_E_EN
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //TEMP_SW
	{NRF_GPIO_PIN_MAP(0,23),OUTPUT,NRF_GPIO_PIN_PULLUP},    //CS_ADS
	{NRF_GPIO_PIN_MAP(0,22), INPUT,NRF_GPIO_PIN_PULLUP},    //DRDY

	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //REF_R1
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //REF_R2
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //REF_R3
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //REF_R4
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //REF_R5
	{UNUSED_PIN, OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //REF_R6
	{UNUSED_PIN, OUTPUT,NRF_GPIO_PIN_PULLDOWN},    //REF_R7
};
#elif PCB_ID == PCB_MULTI_CHANNEL
#define PIN_IMU_IRQ     NRF_GPIO_PIN_MAP(0,29)
static const io_pin iomap[] =
{
	{NRF_GPIO_PIN_MAP(0,6),OUTPUT,NRF_GPIO_PIN_NOPULL},		//LED_R
    {NRF_GPIO_PIN_MAP(0,7),OUTPUT,NRF_GPIO_PIN_NOPULL},		//LED_G
	{NRF_GPIO_PIN_MAP(0,28),OUTPUT,NRF_GPIO_PIN_NOPULL},	//LED_B
	{NRF_GPIO_PIN_MAP(0,25), OUTPUT,NRF_GPIO_PIN_PULLUP},	//MEM_RST
	{NRF_GPIO_PIN_MAP(0,8),OUTPUT,NRF_GPIO_PIN_PULLUP},    	//MEM_CS
	{NRF_GPIO_PIN_MAP(0,5),OUTPUT,NRF_GPIO_PIN_PULLUP},    	//IMU_CS
	{PIN_IMU_IRQ,INPUT,NRF_GPIO_PIN_PULLUP},              	//IMU_INT
	{NRF_GPIO_PIN_MAP(0,20),OUTPUT,NRF_GPIO_PIN_PULLDOWN},  //HEATING_ChA
	{NRF_GPIO_PIN_MAP(0,16),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//HEATING_ChB
	{NRF_GPIO_PIN_MAP(0,22),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//HEATING_ChC
	{NRF_GPIO_PIN_MAP(0,21),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//HEATING_ChD
	{NRF_GPIO_PIN_MAP(1,4),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//HEATING_ChE
	{NRF_GPIO_PIN_MAP(1,2), OUTPUT,NRF_GPIO_PIN_PULLDOWN},	//PD_RESET
	{UNUSED_PIN, OUTPUT,NRF_GPIO_PIN_PULLDOWN},				//V_HEATER_EN
	{NRF_GPIO_PIN_MAP(0,11),OUTPUT,NRF_GPIO_PIN_PULLDOWN},	//TEMP_A
	{NRF_GPIO_PIN_MAP(1,8),OUTPUT,NRF_GPIO_PIN_PULLDOWN},  	//TEMP_B
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},  			//TEMP_EN
	{NRF_GPIO_PIN_MAP(1,0),OUTPUT,NRF_GPIO_PIN_PULLUP},    	//TEMP_E_EN low active
	{NRF_GPIO_PIN_MAP(1,7),OUTPUT,NRF_GPIO_PIN_PULLDOWN},  	//TEMP_SW  high active
	{NRF_GPIO_PIN_MAP(0,13),OUTPUT,NRF_GPIO_PIN_PULLUP},    //CS_ADS
	{NRF_GPIO_PIN_MAP(0,24), INPUT,NRF_GPIO_PIN_PULLUP},    //DRDY
	{NRF_GPIO_PIN_MAP(0,23),OUTPUT,NRF_GPIO_PIN_PULLDOWN},	//REF_R1
	{NRF_GPIO_PIN_MAP(0,2),OUTPUT,NRF_GPIO_PIN_PULLDOWN},  	//REF_R2
	{NRF_GPIO_PIN_MAP(0,31),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//REF_R3
	{NRF_GPIO_PIN_MAP(0,19),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//REF_R4
	{NRF_GPIO_PIN_MAP(0,30),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//REF_R5
	{NRF_GPIO_PIN_MAP(1,5), OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//REF_R6
	{NRF_GPIO_PIN_MAP(1,3), OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//REF_R7
	{UNUSED_PIN, OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 			//BUTTON
};
#elif PCB_ID == PCB_DUAL_CHANNEL
#define PIN_IMU_IRQ     NRF_GPIO_PIN_MAP(0,22)
static const io_pin iomap[] =
{
	{NRF_GPIO_PIN_MAP(0,30),OUTPUT,NRF_GPIO_PIN_NOPULL},	//LED_R
    {NRF_GPIO_PIN_MAP(0,29),OUTPUT,NRF_GPIO_PIN_NOPULL},	//LED_G
	{NRF_GPIO_PIN_MAP(0,31),OUTPUT,NRF_GPIO_PIN_NOPULL},	//LED_B
	{NRF_GPIO_PIN_MAP(1,0), OUTPUT,NRF_GPIO_PIN_PULLUP},	//MEM_RST
	{NRF_GPIO_PIN_MAP(0,16),OUTPUT,NRF_GPIO_PIN_PULLUP},    //MEM_CS
	{NRF_GPIO_PIN_MAP(0,15),OUTPUT,NRF_GPIO_PIN_PULLUP},    //IMU_CS
	{PIN_IMU_IRQ,INPUT,NRF_GPIO_PIN_PULLUP},              	//IMU_INT
	{NRF_GPIO_PIN_MAP(1,3),OUTPUT,NRF_GPIO_PIN_PULLDOWN},  	//HEATING_ChA
	{NRF_GPIO_PIN_MAP(0,19),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//HEATING_ChB
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 				//HEATING_ChC
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 				//HEATING_ChD
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 				//HEATING_ChE
	{UNUSED_PIN, OUTPUT,NRF_GPIO_PIN_PULLDOWN},				//PD_RESET
	{NRF_GPIO_PIN_MAP(0,10), OUTPUT,NRF_GPIO_PIN_PULLDOWN},	//V_HEATER_EN
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},				//TEMP_A
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLDOWN},  			//TEMP_B
	{NRF_GPIO_PIN_MAP(1,5),OUTPUT,NRF_GPIO_PIN_PULLDOWN},  	//TEMP_EN
	{UNUSED_PIN,OUTPUT,NRF_GPIO_PIN_PULLUP},    			//TEMP_E_EN low active
	{NRF_GPIO_PIN_MAP(1,6),OUTPUT,NRF_GPIO_PIN_PULLDOWN},  	//TEMP_SW  high active
	{NRF_GPIO_PIN_MAP(0,11),OUTPUT,NRF_GPIO_PIN_PULLUP},    //CS_ADS
	{NRF_GPIO_PIN_MAP(0,8), INPUT,NRF_GPIO_PIN_PULLUP},    	//DRDY
	{NRF_GPIO_PIN_MAP(0,4),OUTPUT,NRF_GPIO_PIN_PULLDOWN},	//REF_R1
	{NRF_GPIO_PIN_MAP(0,23),OUTPUT,NRF_GPIO_PIN_PULLDOWN},  //REF_R2
	{NRF_GPIO_PIN_MAP(0,28),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//REF_R3
	{NRF_GPIO_PIN_MAP(0,12),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//REF_R4
	{NRF_GPIO_PIN_MAP(0,5),OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//REF_R5
	{UNUSED_PIN, OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 			//REF_R6
	{UNUSED_PIN, OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 			//REF_R7
	{NRF_GPIO_PIN_MAP(0,7), OUTPUT,NRF_GPIO_PIN_PULLDOWN}, 	//BUTTON
};
#endif //DEVKIT_NRF52
static const uint16_t N_IO = sizeof(iomap)/sizeof(io_pin);

#if DEVKIT_NRF52 == 1 //running code using PCA10040 development kit
static pin_irq irqmap[] = {};
#else
static pin_irq irqmap[] =
{
	{PIN_IMU_IRQ,  NRF_GPIO_PIN_PULLUP, NRF_GPIOTE_POLARITY_HITOLO, NULL},
};
#endif
static const uint16_t N_IRQ = sizeof(irqmap)/sizeof(pin_irq);
static IOStatus status = IOST_MAP_ERROR;

static const io_pin * const get_pin(uint16_t pin);
static pin_irq *get_irqpin(uint16_t pin);
static void irq_imu(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);

IOStatus hal_gpio_init(void)
{
    uint16_t i;
    const io_pin *pinmap = iomap;
    const pin_irq *irq = irqmap;
    if(N_IO != MAX_IOPINS)
        status = IOST_MAP_ERROR;
    else //configure pins
    {
        for(i = 0; i < N_IO; i++, pinmap++)
        {
            if(UNUSED_PIN == pinmap->pin)
                continue;
            if(OUTPUT == pinmap->dir)
            {
                nrf_gpio_cfg_output(pinmap->pin);
                if(pinmap->pull==NRF_GPIO_PIN_PULLUP)
                	nrf_gpio_pin_set(pinmap->pin);
                else
                	nrf_gpio_pin_clear(pinmap->pin);
            }
            else
                nrf_gpio_cfg_input(pinmap->pin, pinmap->pull);
        }
        status = IOST_MAP_OK;
    }

    ret_code_t retc;
    if(N_IRQ > 0)
    {
		//configure irq on gpio pins, enable port interrupt
		nrf_drv_gpiote_in_config_t in_config;
		nrf_drv_gpiote_init();
		in_config.is_watcher = false;
		in_config.hi_accuracy = false;
		for(i = 0; i < N_IRQ; i++, irq++)
		{
			in_config.pull = irq->pull;
			in_config.sense = irq->edge;
			if(irq->pin == PIN_IMU_IRQ)
				retc = nrf_drv_gpiote_in_init(irq->pin, &in_config, irq_imu);
			if(retc != NRF_SUCCESS)
			{
				status = IOST_MAP_ERROR;
				break;
			}
		}
    }
    return status;
}

void hal_gpio_deinit(void)
{
    uint16_t i;
    const io_pin *pinmap = iomap;
    pin_irq *irq = irqmap;

    //disable irqs
    for(i = 0; i < N_IRQ; i++, irq++)
    {
        hal_gpio_enable_irq(irq->pin, false);
        irq->callback = NULL;
    }
    status = IOST_MAP_ERROR; //force invalid status
    //restore pins default state
    for(i = 0; i < N_IO; i++, pinmap++)
    {
        if(UNUSED_PIN == pinmap->pin)
            continue;
        nrf_gpio_cfg_default(pinmap->pin);
    }
}

bool hal_gpio_enable_irq(IOPin pin, bool en)
{
    if(IOST_MAP_OK != status)
        return false;

    const io_pin * const io = get_pin(pin);
    if(NULL == io)
        return false;

    pin_irq *irq = get_irqpin(io->pin);

    if(NULL == irq)
        return false;

    nrf_drv_gpiote_in_event_enable(irq->pin, en);

    return true;
}

bool hal_gpio_irq_callb(IOPin pin, pinirq_callb_t callb)
{
    if(IOST_MAP_OK != status)
        return false;
    if(NULL == callb)
        return false;
    const io_pin * const io = get_pin(pin);

    if(NULL == io)
        return false;

    pin_irq *irq = get_irqpin(io->pin);
    if(NULL == irq)
        return false;

    irq->callback = callb;
    return true;
}

void hal_gpio_set(IOPin pin)
{
    if(IOST_MAP_OK != status)
        return;
    const io_pin * const io = get_pin(pin);
    if(NULL == io || (INPUT == io->dir))
        return;
    nrf_gpio_pin_set(io->pin);
}

void hal_gpio_clr(IOPin pin)
{
    if(IOST_MAP_OK != status)
        return;
    const io_pin * const io = get_pin(pin);
    if(NULL == io || (INPUT == io->dir))
        return;
    nrf_gpio_pin_clear(io->pin);
}

uint32_t hal_gpio_read(IOPin pin)
{
    const io_pin * const io = get_pin(pin);
    if(NULL == io)
        return 0;
    return nrf_gpio_pin_read(io->pin);
}

void hal_gpio_toggle(IOPin pin)
{
    if(IOST_MAP_OK != status)
        return;
    const io_pin * const io = get_pin(pin);
    if(NULL == io || (INPUT == io->dir))
        return;
    nrf_gpio_pin_toggle(io->pin);
}

void hal_gpio_write(IOPin pin, uint8_t level)
{
    if(IOST_MAP_OK != status)
        return;
    const io_pin * const io = get_pin(pin);
    if(NULL == io || (INPUT == io->dir))
        return;
    nrf_gpio_pin_write(io->pin, level);
}

static const io_pin * const get_pin(uint16_t pin)
{
    if(pin >= N_IO)
        return NULL;
    const io_pin * const io = &iomap[pin];
    if(UNUSED_PIN == io->pin)
        return NULL;
    return io;
}

static pin_irq *get_irqpin(uint16_t pin)
{
    pin_irq *irq = irqmap;
    uint8_t i;

    for(i = 0; i < N_IRQ; i++, irq++)
    {
        if(irq->pin == pin)
            return irq;
    }
    return NULL;
}

static void irq_imu(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    pin_irq *irq = get_irqpin(PIN_IMU_IRQ);
    if((NULL != irq) && (NULL != irq->callback))
    {
        irq->callback();
    }
}


#endif //UC_ID == UC_NRF52832

