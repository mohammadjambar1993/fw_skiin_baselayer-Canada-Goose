/*
 * ina231.c:  Driver for I2C current sensor ina231
 *
 *  Created on: Aug 1 , 2019
 *      Author: Myant
 */
#include "drivers/ina231.h"

#include "../diagnostic.h"
#include "hal_i2c.h"
#include "mya_util.h"
#include "logger.h"
#include "appconfig.h"
//#include "hal_pwm.h"
#include "hal_gpio.h"

//20us to send 1 byte at 400KHz +  1b address + 1b tolerance
#define TIME_XFER(NBYTES)   (1500*(NBYTES+2))

//I2C address for chips
#define CUR_I2C_ADDR_1        0x40  //CH1, IAN231 U13
#define CUR_I2C_ADDR_2        0x41  //CH2, IAN231 U14
#define CUR_I2C_ADDR_3        0x44  //CH3, IAN231 U18
#define CUR_I2C_ADDR_4        0x45  //CH4, IAN231 U25
#define CUR_I2C_ADDR_5        0x42  //CH5, IAN231 U26  not used this time
#if PCB_ID == PCB_MULTI_CHANNEL
static const uint8_t cur_i2c_list[CUR_CH_NUM]={CUR_I2C_ADDR_1,CUR_I2C_ADDR_2,\
		CUR_I2C_ADDR_3 ,CUR_I2C_ADDR_4, CUR_I2C_ADDR_5};
#elif PCB_ID == PCB_DUAL_CHANNEL
static const uint8_t cur_i2c_list[CUR_CH_NUM]={CUR_I2C_ADDR_1,CUR_I2C_ADDR_2};
#endif

//registers addresses
#define CUR_REG_Configuration      0x00
#define CUR_REG_Shunt_Voltage      0x01
#define CUR_REG_Bus_Voltage        0x02
#define CUR_REG_Power              0x03
#define CUR_REG_Current            0x04
#define CUR_REG_Calibration        0x05
#define CUR_REG_Mask_Enable        0x06
#define CUR_REG_Alert_Limit        0x07

//configuration data and setup value
#define CUR_AVG_SET         0<<9      //1, num of samples for averaging
#define CUR_VBUS_SAMP_TIME  6<<6     //Vbus sampling time 4.156 ms
#define CUR_VSH_SAMP_TIME   7<<3     //Vshunt sampling time 8.156 ms
#define CUR_OP_MODE         7        //Shunt and bus, continue
#define CUR_SOL             1<<15    //assert Alert pin when shunt overvoltage
#define CUR_APOL            0        //alert polarity is low
#define CUR_LEN             1       //alert latch enabled
#define CUR_AFF_MASK        1<<4   //check if alert asserted
#define CUR_CVRF_MASK       1<<3    //check if conversion ready
#define CUR_OVF_MASK        1<<2    //check if current calculation overflow


#define CUR_CURRENT_SCALE      2048.0    // in mA
#define CUR_CURRENT_LSB        0.0625   // in mA:   2048/2exp15
#define CUR_SHUNT_V_LSB        2.5    // in uV
#define CUR_BUS_V_LSB          1.25    // in mV

static volatile bool txend = false;

static cur_status cur_read_reg(uint8_t i2c_addr, uint8_t reg, uint16_t *data);
static cur_status cur_write_reg(uint8_t i2c_addr, uint8_t reg, uint16_t data);
static void irq_i2c(I2CStatus evt);

/*notes:this function does not change endianness  */
static cur_status cur_read_reg(uint8_t i2c_addr, uint8_t reg, uint16_t *data)
{
    I2CStatus st;
    bool txok;
    if(data==NULL){
    	log_error("[cur] errro: null pointer\r\n");
       	return CUR_ST_FAIL;
    }
    hal_i2c_set_irq_callback(irq_i2c);
    txok = txend = false;
    st = hal_i2c_tx(i2c_addr, &reg, 1, true);//no stop for re_start
    if(I2CST_OP_OK == st)
    {
        txok = util_check_flag_us(&txend, TIME_XFER(1));
        if(txok||txend)
        {
            txok = txend = false;
            st = hal_i2c_rx(i2c_addr, (uint8_t*)data, 2);
            if(I2CST_OP_OK == st)
                txok = util_check_flag_us(&txend, TIME_XFER(2));
        }
    }

    if((txok||txend) && (I2CST_OP_OK == st))
        return CUR_ST_OK;
    diag_inc_flag(FLG_INA_READ);
    return CUR_ST_FAIL;
}

static cur_status cur_write_reg(uint8_t i2c_addr, uint8_t reg, uint16_t data)
{
    I2CStatus st;
    bool txok;
    uint8_t cmd[] = {reg, (uint8_t)(data>>8),(uint8_t)data};
    hal_i2c_set_irq_callback(irq_i2c);
    txok = txend = false;
    st = hal_i2c_tx(i2c_addr, cmd, sizeof(cmd), true);
    if(I2CST_OP_OK == st)
        txok = util_check_flag_us(&txend, TIME_XFER(sizeof(cmd)));

    if((txok||txend) && (I2CST_OP_OK == st))
         return CUR_ST_OK;
    diag_inc_flag(FLG_INA_WRITE);
    return CUR_ST_FAIL;
}

static bool config_i2c(void)
{
    I2CStatus status;
    status = hal_i2c_init();
    if(I2CST_OP_OK == status)
    {
        status = hal_i2c_set_irq_callback(irq_i2c);
        if(I2CST_OP_OK == status)
        {
            hal_i2c_enableIRQ(true);
            hal_i2c_enable(true);
        }
    }
    return (I2CST_OP_OK == status);
}


/* initialize ina231:shunt_res is shunt resistance in mohm for current sensing,
 *  cur_limit is over current in mA. set them in continuous mode */
bool cur_sensor_init(uint8_t ch, uint16_t shunt_res, uint16_t cur_limit)
{
	static bool i2c_initialized = false;
	cur_status st;
#if DEVKIT_NRF52 == 1
    return true;
#endif
    uint16_t data;
    /*initialize current sensors */
    if(ch>=CUR_CH_NUM)
    {
    	return false;
    }

    if(!i2c_initialized)
    {
    	i2c_initialized = config_i2c();
    	if(!i2c_initialized)
    	{
    		log_error("[cur] failed to initialize i2c port");
    		return false;
    	}
    }

	//8.244ms for Vsh,8.244ms for Vbus conversion, averaging:4, Vsh,Vbus continuous
	st = cur_write_reg(cur_i2c_list[ch], CUR_REG_Configuration, CUR_AVG_SET\
			|CUR_VBUS_SAMP_TIME|CUR_VSH_SAMP_TIME|CUR_OP_MODE);
	if(CUR_ST_OK != st)
	{
		log_error("[cur] error conf %s, %d,%d", __FILE__, __LINE__,ch);
		return (CUR_ST_OK == st);
	}
	/* config calibration register */
	data=5120.0/(CUR_CURRENT_LSB*(double)shunt_res);
	st = cur_write_reg(cur_i2c_list[ch], CUR_REG_Calibration, data);
	if(CUR_ST_OK != st)
	{
		log_error("[cur] error calib set %s, %d,%d", __FILE__,__LINE__,ch);
		return (CUR_ST_OK == st);
	}
	//read mask enable register to clear status
	st = cur_read_reg(cur_i2c_list[ch], CUR_REG_Mask_Enable, &data);
	if(CUR_ST_OK != st)
	{
		log_error("[cur] error read mask %s,%d,%d", __FILE__,__LINE__,ch);
		return (CUR_ST_OK == st);
	}

	//config mask enable register
	st = cur_write_reg(cur_i2c_list[ch], CUR_REG_Mask_Enable, \
			CUR_SOL|CUR_APOL|CUR_LEN);
	if(CUR_ST_OK != st)
	{
		log_error("[cur] error config mask %s,%d,%d", __FILE__,__LINE__,ch);
		return (CUR_ST_OK == st);
	}
	/*config alert limit reigister */
	data=((double)cur_limit)*((double)shunt_res)/CUR_SHUNT_V_LSB;
	st = cur_write_reg(cur_i2c_list[ch], CUR_REG_Alert_Limit, data);
	if(CUR_ST_OK != st)
	{
		log_error("[cur] error alert set %s,%d,%d", __FILE__,__LINE__,ch);
		return (CUR_ST_OK == st);
	}

    return (CUR_ST_OK == st);
}

//check alert status, true for alert asserted
cur_status cur_alert_status(cur_channel_t ch, uint8_t * short_status)
{
	cur_status st;
	uint16_t data;
	if(short_status==NULL)
		return CUR_ST_FAIL;
	//read mask enable register to clear status
	st = cur_read_reg(cur_i2c_list[ch], CUR_REG_Mask_Enable, &data);
	if(CUR_ST_OK != st)
	{
	    return st;
	}
	util_change_endianness(&data, sizeof(uint16_t));
	if((data&CUR_AFF_MASK)==CUR_AFF_MASK)  //alert asserted
		(*short_status)=1;
	else
		(*short_status)=0;
	return st;
}

//it take about 9ms for the chip to produce data of Vshunt and Vbus
cur_status cur_start_conversion(cur_channel_t ch)
{
	cur_status st;
	if(ch>=CUR_CH_NUM){
		log_debug("ch num wrong");
		return CUR_ST_FAIL;
	}
	//start one-shot conversion
	//2x4.156ms for Vsh,Vbus conversion,no averaging, Vsh,Vbus triggerd
	st= cur_write_reg(cur_i2c_list[ch], CUR_REG_Configuration, CUR_AVG_SET\
	    			|CUR_VBUS_SAMP_TIME|CUR_VSH_SAMP_TIME|CUR_OP_MODE);
	if(CUR_ST_OK != st)
	{
	    log_error("[cur] error oneshot %s, %d", __FILE__, __LINE__);
	}
	return st;
}
/*the readback current is in mA */
cur_status cur_read_current(cur_channel_t ch, int16_t *data)
{
	cur_status st;
	uint16_t temp;
	if(data==NULL||ch>=CUR_CH_NUM)
	{
		    log_error("[cur] wrong arguments\r\n");
		    return CUR_ST_FAIL;
	}
	st=cur_read_reg(cur_i2c_list[ch], CUR_REG_Current,	&temp);
	if(CUR_ST_OK != st)
		return st;
	util_change_endianness(&temp, sizeof(uint16_t));
	//calculate current, in mA
	*data= (int16_t) ( ( (double)((int16_t)temp) )*CUR_CURRENT_LSB  );
	return st;
}

cur_status cur_read_voltage(cur_channel_t ch, uint16_t *data)
{
	cur_status st;
	if(data==NULL||ch>=CUR_CH_NUM)
	{
			    log_error("[cur] wrong arguments\r\n");
			    return CUR_ST_FAIL;
	}
	st=cur_read_reg(cur_i2c_list[ch], CUR_REG_Bus_Voltage,	data);
	if(CUR_ST_OK != st)
		return st;
	util_change_endianness(data, sizeof(uint16_t));
	//calculate voltage, in mV
	*data=(uint16_t)(((double)(*data))*CUR_BUS_V_LSB);
	return st;
}

static void irq_i2c(I2CStatus evt)
{
    if(I2CST_IRQ_OP_DONE == evt)
    {
        txend = true;
    }
    else if(I2CST_IRQ_ADDR_NACK == evt || I2CST_IRQ_DATA_NACK == evt)
    {
    	diag_inc_flag(FLG_I2C_IRQ_INA);
        log_error("[cur] i2c error: %d", evt);
    }
}
