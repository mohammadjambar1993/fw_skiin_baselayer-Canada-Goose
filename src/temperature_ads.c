/*
 * temperature_ads.c
 *
 *  Created on: Sep 14, 2020
 *      Author: Jeffrey Zhu
 */
#include "temperature_ads.h"

#include "hal_gpio.h"
#include "ADS1220.h"
#include "tskctrl.h"
#include "logger.h"
#include "hal_spi.h"
#include <string.h>
#include "diagnostic.h"
#include "mya_util.h"
#include "hal_gpio.h"
#include "heat_ctrl.h"
#include "wdt.h"
#include "drivers/mx25r6435.h"
#include "crc16.h"
#include "semphr.h"
#include "cmd_protocol.h"
#include "memory.h"

#define ENABLE_LOGGER_BANDCFG
#ifdef ENABLE_LOGGER_BANDCFG
	#include "util/logger.h"
	#define logtag	"[ads] "
	#define _debug(...) 	log_debug(logtag __VA_ARGS__)
	#define _info(...)		log_info(logtag __VA_ARGS__)
	#define _warn(...)		log_warn(logtag __VA_ARGS__)
	#define _error(...)		log_error(logtag __VA_ARGS__)
#else
	#define _debug
	#define _info
	#define _warn
	#define _error
#endif


#define FULL_SCALE  ((double)8388608.0)  //2 ^23 for 24 bit adc
#if DEVKIT_NRF52 == 1 //running code using PCA10040 development kit

#define REF_RES     1500.0    //reference resistance(1.5k) for reference voltage devided by Gain
#define PGA_BYPASS 1      //must bypass PGA to measure unique signal
#define PGA_GAIN   0b000  // Gain = 1
#define MUX        0b0000 // AINP = AIN0, AINN = AIN1
#define REG0_CONF  (PGA_BYPASS|(PGA_GAIN<<1)|(MUX<<4))

#define BCS        0       //burn_out current source off
#define TS         0       //Disables temperature sensor
#define CM         0       //Single-shot mode
#define OP_MODE    0b00    //Normal mode (256-kHz modulator clock, default)
#define DR         0b101   // 600 SPS
#define REG1_CONF  (BCS|(TS<<1)|(CM<<2)|(OP_MODE<<3)|(DR<<5))

#define IDAC       0b110    // 1000 μA
#define PSW        0        //0 : Switch is always open (default)
#define FILTER_50  0b00     // No 50-Hz or 60-Hz rejection (default)
#define VREF       0b01  //External reference selected using dedicated REFP0 and REFN0 inputs
#define REG2_CONF  (IDAC|(PSW<<3)|(FILTER_50<<4)|(VREF<<6))

#define DRDYM      1         //Data ready is indicated simultaneously on DOUT/DRDY and DRDY
#define I2MUX      0b000   // IDAC2 disabled (default)
#define I1MUX      0b011   // IDAC1 connected to AIN2
#define REG3_CONF  (DRDYM<<1|(I2MUX<<2)|(I1MUX<<5))
#else

#define REF_RES    1000//((double)1000.0)    //reference resistance(1.0k) for reference voltage devided by Gain

#define PGA_BYPASS 1      //must bypass PGA to measure unique signal
#define PGA_GAIN   0b000  // Gain = 1
#define MUX        0b1001 // 1001 : AINP = AIN1, AINN = AVSS
#define REG0_CONF  (PGA_BYPASS|(PGA_GAIN<<1)|(MUX<<4))

#define BCS        0       //burn_out current source off
#define TS         0       //Disables temperature sensor
#define CM         0       //Single-shot mode
#define OP_MODE    0b00    //Normal mode (256-kHz modulator clock, default)
#define DR         0b100   // 330 SPS
#define REG1_CONF  (BCS|(TS<<1)|(CM<<2)|(OP_MODE<<3)|(DR<<5))
#define REG1_CONF_TS  (BCS|(1<<1)|(CM<<2)|(OP_MODE<<3)|(DR<<5))  //enable temperature sensor

#define IDAC       0b110    // 1000 μA
#define PSW        0        //0 : Switch is always open (default)
#define FILTER_50  0b00     // No 50-Hz or 60-Hz rejection (default)
#define VREF       0b01  //External reference selected using dedicated REFP0 and REFN0 inputs
#define REG2_CONF  (IDAC|(PSW<<3)|(FILTER_50<<4)|(VREF<<6))

#define DRDYM      1         //Data ready is indicated simultaneously on DOUT/DRDY and DRDY
#define I2MUX      0b000   // IDAC2 disabled (default)F
#define I1MUX      0b001   // IDAC1 connected to AIN0/REFP1
#define REG3_CONF  (DRDYM<<1|(I2MUX<<2)|(I1MUX<<5))

#endif //DEVKIT_NRF52

#define MAX_REGS   	  	  4
#define ADS_CONFIG_ADDR   0x001000
#define ADS_PARAMS_PAGE	  1
#define LEN_BUFFER		  256
#define LEN_WRITE_OP	  64 //maximum bytes to write in a single flash operation

static uint8_t ads_config_buffer[SENS_MODE_PARAMS];
static uint8_t local_buffer[LEN_BUFFER];
static uint16_t crc=0;

static uint8_t mode=SENS_OP_SINGLE_END; //default mode
static uint8_t default_conf[SENS_DEFAULT_PARAMS]={SENS_OP_SINGLE_END,REG0_CONF,REG1_CONF,REG2_CONF,REG3_CONF};
static uint8_t sampling_int=0;  //sampling time in ms
static bool temp_inited=false;
static uint8_t gain=1;
static double res_match=0;  //the match resistor for differential method

static cmd_ads_config_t config_store;

static struct ads_dev_t ads_callb;
static int32_t calibrate_val=0;
static bool spi_tx_end = false;
static spi_port_t spi = NULL;

#define TASK_SENS_MEAS 1000   //the interval for task_sens_meas
static int16_t temp_pcba=0x7fff; //store temperature of PCBA board

static void calibrate_ads(void) __attribute__((unused));
static bool config_spi(void);
static void irq_spi_tx(void);
static int8_t tx_spi(uint8_t reg_addr, uint8_t *data, uint8_t len);
static int8_t rx_spi(uint8_t reg_addr, uint8_t *data, uint8_t len);
static void set_temp_ref(uint8_t ref_conf);
static bool adjust_dr(uint8_t *data);
static void calculate_gain(uint8_t data);
static bool enable_sensor(bool en);
static app_status_t write_ads_config_to_flash(void);
static void read_ads_config_from_flash(void);
static bool ads_config(uint8_t *mode, uint8_t len);

// ################ functions to interface with ADS1220.c driver

static int8_t tx_spi(uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    bool txok;
    int8_t success = ADS1220_E_COM_FAIL;
    uint8_t buffer[len + 1];

    buffer[0] = reg_addr;
	if(data!=NULL)
		memcpy(&buffer[1], data, len);
	hal_gpio_clr(CS_ADS);
	spi_tx_end = false;
	if(SPIST_OP_OK == hal_spi_tx(spi, buffer, NULL, len + 1))
	{
		txok = util_check_flag_us(&spi_tx_end, len + 1);
		if(txok || spi_tx_end)
		{
			success=ADS1220_OK;
		}
		else
		{
			diag_inc_flag(FLG_AD_SPI_TIMEOUT);
			log_error("[AD] SPI tx timed out!");
			success = ADS1220_E_COM_FAIL;
		}
	}
	else
	{
		diag_inc_flag(FLG_AD_SPI_TXERROR);
		log_error("[AD] SPI failed to tx");
		success = ADS1220_E_COM_FAIL;
	}
    hal_gpio_set(CS_ADS);
    return success;
}

static int8_t rx_spi(uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    bool txok;
    int8_t success = ADS1220_E_COM_FAIL;
    uint8_t buffer_tx[len + 1];
    uint8_t buffer_rx[len + 1];

    buffer_tx[0] = reg_addr;
	hal_gpio_clr(CS_ADS);
	spi_tx_end = false;
	if(SPIST_OP_OK == hal_spi_tx(spi, buffer_tx, buffer_rx, len + 1))
	{
		txok = util_check_flag_us(&spi_tx_end, len + 1);
		if(txok || spi_tx_end)
		{
			success=ADS1220_OK;
			memcpy(data, &buffer_rx[1], len);
		}
		else
		{
			diag_inc_flag(FLG_AD_SPI_TIMEOUT);
			log_error("[AD] SPI rx timed out!");
			success = ADS1220_E_COM_FAIL;
		}
	}
	else
	{
		diag_inc_flag(FLG_AD_SPI_TXERROR);
		log_error("[AD] SPI failed to rx");
		success = ADS1220_E_COM_FAIL;
	}
    hal_gpio_set(CS_ADS);
    return success;
}

static void delay_ms(uint32_t period)
{
    vTaskDelay(pdMS_TO_TICKS(period));
}

bool ads_init(void)
{
	ads_callb.read = rx_spi;
    ads_callb.write = tx_spi;
    ads_callb.delay_ms = delay_ms;

    if(!config_spi())
    {
    	log_error("[tempads] failed to config SPI");
    	return false;
    }

    read_ads_config_from_flash();

    if(!ads_config(ads_config_buffer, SENS_DEFAULT_PARAMS))
    {
    	log_error("[tempads] temperature_config failed");
    	return false;
    }
    //calibrate_ads();  //seems not useful because the value varies vey large
    temp_inited=true;
    return true;
}

static bool ads_config(uint8_t *data, uint8_t len)
{
	uint8_t temp_write[MAX_REGS];
	uint8_t temp_read[MAX_REGS];
	uint8_t value;

	if((!spi)||(data==NULL)||(len!=SENS_DEFAULT_PARAMS))
	{
		return false;
	}

	if((data[0]!=SENS_OP_DIFF_END)&&(data[0]!=SENS_OP_SINGLE_END))
	{
		return false;
	}

	memcpy(temp_write,&(data[1]),MAX_REGS);
	temp_write[1]=temp_write[1]&0xfd;   //disable temperature sensor
	if(!adjust_dr(&temp_write[1])) //adjust data rate if it is too slow
	{
		return false;
	}

	calculate_gain(temp_write[0]);
	if(data[0]==SENS_OP_DIFF_END) //need to config resistor ladder
	{
		mode=SENS_OP_DIFF_END;
		set_temp_ref(data[SENS_MODE_PARAMS-1]);
	}
	else
		mode=SENS_OP_SINGLE_END;

	if(ADS1220WriteRegister(ADS1220_0_REGISTER, MAX_REGS, temp_write,&ads_callb)!=ADS1220_OK)
		return false;

	if(ADS1220ReadRegister(ADS1220_0_REGISTER, MAX_REGS, temp_read,&ads_callb)!=ADS1220_OK)
		return false;

	if(memcmp(temp_read, temp_write, sizeof(temp_read)) != 0)
	{
		_warn("ads configuration not written correctly");
		return false;
	}

	// Save the copy of ADS config here to make sure it is sending the right values
	value = temp_read[0];
	config_store.gain = (value>>1)&0x07;
	config_store.PGA = value&0x01;

	value = temp_read[1];
	config_store.SPS = value>>5;
	config_store.oprt_mode = (value>>3)&0x03;
	config_store.conv_mode = (value>>2)&0x01;
	config_store.temp_sensor_mode = (value>>1)&0x01;
	config_store.BCSource = value&0x01;

	value = temp_read[2];
	config_store.Vref = value>>6;
	config_store.filter_config = (value>>4)&0x03;
	config_store.power_switch = (value>>3)&0x01;
	config_store.IDACsetting = value&0x07;

	memcpy(&config_store.Rref, &ads_config_buffer[5], sizeof(uint16_t));

	return true;
}

uint8_t ads_temperature_get_mode(void)
{
	return mode;
}

#define OP_MODE_SHIFT  3
#define DR_SHIFT       5
#define DR_MASK        0x1f
#define TURBO_MODE     2
/* adjust data rate if it is too slow */
static bool adjust_dr(uint8_t *data)
{
	uint8_t op_mode=0, data_rate=0, temp=*data;

	op_mode=(temp>>OP_MODE_SHIFT)&0x03;
	data_rate=temp>>DR_SHIFT;
	if(op_mode==3)  //invalid mode
		return false;
	if(op_mode==TURBO_MODE)
	{
		if(data_rate<=0b011)
		{
			data_rate=0b011;
			sampling_int=3; //sampling interval is 3 ms
		}
		else if(data_rate==0b100)
			sampling_int=2; //sampling interval is 2 ms
		else if(data_rate!=0b111)
			sampling_int=1; //sampling interval is 3 ms
		else  //invalid data rate
			return false;
	}
	else
	{
		if(data_rate<=0b100)
		{
			data_rate=0b100;
			sampling_int=4; //sampling interval is 4 ms
		}
		else if(data_rate!=0b111)
			sampling_int=2; //sampling interval is 2 ms
		else  //invalid data rate
			return false;
	}
	*data= (temp&DR_MASK)|(data_rate<<DR_SHIFT);
	return true;
}

#define GAIN_SHIFT  1
#define GAIN_MASK   0b0111
static void calculate_gain(uint8_t data)
{
	uint8_t gain_bits=0;

	gain_bits=(data>>GAIN_SHIFT)&GAIN_MASK;
	switch(gain_bits)
	{
		case 1:
			gain=2;
		break;
		case 2:
			gain=4;
		break;
		case 3:
			gain=8;
		break;
		case 4:
			gain=16;
		break;
		case 5:
			gain=32;
		break;
		case 6:
			gain=64;
		break;
		case 7:
			gain=128;
		break;
		default:
			gain=1;
		break;
	}
}

static bool config_spi(void)
{
    SPIStatus status;
    if(spi)
    {
        return false;
    }
    spi = hal_spi_init(SPI_ID2);
    if(spi)
    {
        status = hal_spi_set_tx_callback(spi, irq_spi_tx);
        if(SPIST_OP_OK == status)
        {
            hal_spi_enable_irq_tx(spi, true);
            hal_spi_enable(spi, true);
        }
        else
        {
        	spi = NULL;
        }
    }
    return (NULL != spi);
}

#define TIMES_AVG   50
#define ONE_SHOT_TIME 4 //5ms corresponding to 600SPS
static void calibrate_ads(void)
{
	uint8_t pre_conf, temp, i;
	int32_t data=0;
	if(!spi)
		return;
	ADS1220ReadRegister(ADS1220_0_REGISTER, 0x01, &pre_conf,&ads_callb);
	/* clear prev value; */
	temp = pre_conf & 0x0f;
	temp |= 0xe0;  //AINP and AINN shorted to (AVDD + AVSS) / 2
	/* write the register value containing the new value back to the ADS */
	ADS1220WriteRegister(ADS1220_0_REGISTER, 0x01, &temp,&ads_callb);
	/*start TIMES_AVG times of one-shot adc and average readout*/
	calibrate_val=0;
	for(i=0;i<TIMES_AVG; i++)
	{
		/* send start command to start one-shot A/D conversion */
		ADS1220SendStartCommand(&ads_callb);
		util_blocking_delay_ms(ONE_SHOT_TIME); //wait 5ms for adc completion
		ADS1220ReadData(&data, &ads_callb);
		calibrate_val+=data;
	}
	calibrate_val/=TIMES_AVG;
	/* write back previous value */
	ADS1220WriteRegister(ADS1220_0_REGISTER, 0x01, &pre_conf,&ads_callb);
	return;
}

int16_t ads_get_resistance(void)
{
	int32_t data=0;
	uint8_t i=0;
	int16_t res=0;
	uint16_t Rref=0;

	if(!temp_inited)
		return 0x7fff; //invalid temperature value
	// Get Rref value from BLE
	memcpy(&Rref, &config_store.Rref, sizeof(uint16_t));
	/* send start command to start one-shot A/D conversion */
	ADS1220SendStartCommand(&ads_callb);
	while(hal_gpio_read(DRDY))  //wait until data ready
	{
		if(i>=sampling_int)  //the sensor may be wrong since adc should be finished within sampling interval
		{
			log_error(">> fail to get temperature reading <<\r\n");
			return 0x7fff; //invalid temperature value
		}
		util_blocking_delay_ms(1);  //wait for 1ms till switching finished
		i++;
	}
	ADS1220ReadData(&data, &ads_callb);
	if(mode==SENS_OP_SINGLE_END)
		res=(int16_t)(100*(((double)data)/(FULL_SCALE/(Rref/((double)gain)))));//be careful for overflow
	else
		res=(int16_t)(100*(((double)data)/(FULL_SCALE/(Rref/((double)gain)))))+(int16_t)(100.0*res_match);//be careful for overflow
	return res;
}

#define REF_MASK_R1 1
#define REF_MASK_R2 2
#define REF_MASK_R3 4
#define REF_MASK_R4 8
#define REF_MASK_R5 16
#define REF_MASK_R6 32
#define REF_MASK_R7 64
#define REF_R1_VAL 1.0     //resistance in ohm
#define REF_R2_VAL 2.0
#define REF_R3_VAL 4.02
#define REF_R4_VAL 8.06
#if PCB_ID == PCB_MULTI_CHANNEL
#define REF_R5_VAL 16.0
#elif PCB_ID == PCB_DUAL_CHANNEL
#define REF_R5_VAL 18.0
#endif
#define REF_R6_VAL 32.4
#define REF_R7_VAL 63.4
/*control ref_r# to set ref resistance for differential measurment method*/
static void set_temp_ref(uint8_t ref_conf)
{
	uint8_t control=0;

	res_match=0;
	control=ref_conf&REF_MASK_R1;
	if(control)
		res_match+=REF_R1_VAL;
	hal_gpio_write(REF_R1, control);  //set control pin ref_r1

	control=ref_conf&REF_MASK_R2;
	if(control)
		res_match+=REF_R2_VAL;
	hal_gpio_write(REF_R2, control);  //set control pin ref_r2

	control=ref_conf&REF_MASK_R3;
	if(control)
		res_match+=REF_R3_VAL;
	hal_gpio_write(REF_R3, control);  //set control pin ref_r3

	control=ref_conf&REF_MASK_R4;
	if(control)
		res_match+=REF_R4_VAL;
	hal_gpio_write(REF_R4, control);  //set control pin ref_r4

	control=ref_conf&REF_MASK_R5;
	if(control)
		res_match+=REF_R5_VAL;
	hal_gpio_write(REF_R5, control);  //set control pin ref_r5
#if PCB_ID == PCB_MULTI_CHANNEL
	control=ref_conf&REF_MASK_R6;
	if(control)
		res_match+=REF_R6_VAL;
	hal_gpio_write(REF_R6, control);  //set control pin ref_r6

	control=ref_conf&REF_MASK_R7;
	if(control)
		res_match+=REF_R7_VAL;
	hal_gpio_write(REF_R7, control);  //set control pin ref_r7
#endif
}

/*read temperature from sensor inside ADS1220*/
int16_t ads_get_PCB_temperature(void)
{
	int32_t data=0;
	int16_t temp;
	uint8_t i=0;
	float celcius;
	int8_t ret;

	if(!temp_inited)
	{
		temp_pcba=0x7fff; //invalid temperature value
		return temp_pcba;
	}
	if(!enable_sensor(true))//enable internal sensor
	{
		temp_pcba=0x7fff; //invalid temperature value
		return temp_pcba;
	}
	ret = ADS1220SendStartCommand(&ads_callb);
	sampling_int = 100;
	while(hal_gpio_read(DRDY))  //wait until data ready
	{
		if(i>=sampling_int)  //the sensor may be wrong since adc should be finished within sampling interval
		{
			log_error("fail to get board temperature: %d, %d", sampling_int, ret);
			temp_pcba=0x7fff; //invalid temperature value
			return temp_pcba;
		}
		util_blocking_delay_ms(1);  //wait for 1ms till switching finished
		i++;
	}
	ADS1220ReadData(&data, &ads_callb);
	enable_sensor(false);//disable internal sensor
	temp=(int16_t)((data>>10)&0xffff);
	celcius=(float)(temp)*0.03125;
	//log_debug("Board_Raw=%x, temp=%x, degree=%f", data,temp,celcius ); //be careful for overflow
	temp_pcba=(int16_t)(celcius*100);

	return temp_pcba;
}

static bool enable_sensor(bool en)
{
	uint8_t conf, pre_conf;

	if(ADS1220ReadRegister(ADS1220_1_REGISTER, 0x01, &pre_conf,&ads_callb)!=ADS1220_OK)
		return false;
	if(en)  //enable internal temp sensor
	{
		conf=pre_conf|0x02;//enable temperature sensor
		if(ADS1220WriteRegister(ADS1220_1_REGISTER, 1, &conf,&ads_callb)!=ADS1220_OK)
			return false;
	}
	else
	{
		conf=pre_conf&0xfd;  //disable temperature sensor
		if(ADS1220WriteRegister(ADS1220_1_REGISTER, 1, &conf,&ads_callb)!=ADS1220_OK)
			return false;
	}
	return true;
}

static void irq_spi_tx(void)
{
    //log_debug("[x] irq");
    spi_tx_end = true;
}

app_status_t ads_write_config(cmd_ads_config_t *config)
{
	uint8_t ads_reg_write[MAX_REGS];
	uint8_t ads_reg_verify[MAX_REGS];
	/* writing_config is a protection variable to avoid the writing
	  processes called twice in a multi threaded system */
	static bool writing_config = false;


	if(writing_config)
	{
		_warn("ads configuration already running");
		return APPST_INVALID_STATE;
	}

	if(!spi)
		return APPST_ERROR;

	writing_config = true;

	ads_reg_write[0] = ((config->PGA)|(config->gain<<1)|(MUX<<4));
	ads_reg_write[1] = (config->SPS<<5)|(config->oprt_mode<<3)|(config->conv_mode<<2)
			|(config->temp_sensor_mode<<1)|(config->BCSource);
	ads_reg_write[2] = (config->Vref<<6)|(config->filter_config<<4)
			|(config->power_switch<<3)|(config->IDACsetting);
	ads_reg_write[3] = REG3_CONF;

	if(ADS1220WriteRegister(ADS1220_0_REGISTER, MAX_REGS, ads_reg_write,&ads_callb)!=ADS1220_OK)
	{
		_warn("Write config to ADS failed");
		writing_config = false;
		return APPST_ERROR;
	}
	// After writing the configuration to the ADS, read the values immediately
	//and save into a buffer to compare the written values.
	if(ADS1220ReadRegister(ADS1220_0_REGISTER, MAX_REGS, ads_reg_verify,&ads_callb)!=ADS1220_OK)
	{
		_warn("Read config from ADS failed");
		writing_config = false;
		return APPST_ERROR;
	}

	if(memcmp(ads_reg_verify, ads_reg_write, sizeof(ads_reg_verify)) != 0)
	{
		_warn("ads configuration not written correctly");
		writing_config = false;
		return APPST_ERROR;
	}

	//Write the values into the memory
	local_buffer[0] = mode;
	memcpy(&local_buffer[1], ads_reg_write, sizeof(ads_reg_write));
	memcpy(&local_buffer[5], &config->Rref, sizeof(uint16_t));

	if(write_ads_config_to_flash()!=APPST_SUCCESS)
	{
		writing_config = false;
		return APPST_ERROR;
	}

	writing_config = false;
	return APPST_SUCCESS;
}

app_status_t ads_read_config(cmd_ads_config_t *config_r)
{

	if(NULL == config_r)
	{
		_warn("Invalid parameters for ads configuration");
		return APPST_INVALID_PARAM;
	}
	cmd_ads_config_t *ptr = &config_store;

	memcpy(config_r, ptr, sizeof(cmd_ads_config_t));

	return APPST_SUCCESS;
}

static app_status_t write_ads_config_to_flash(void)
{
	uint32_t addr;
	memory_status_t st;

	_info("Writing ads config into flash");

	addr = ADS_CONFIG_ADDR;
	st = mem_erase_page(addr);
	if(MEMST_OK != st)
	{
		_error("failed to erase sector %d, %d", addr, st);
		return APPST_ERROR;
	}
	util_blocking_delay_ms(100); //erase page takes ~85ms to complete
	//copy buffer to parmaters array
	memset(ads_config_buffer, 0, sizeof(ads_config_buffer));
	memcpy(ads_config_buffer, local_buffer, sizeof(ads_config_buffer));
	//calculate crc
	crc = 0;
	crc = crc16_compute((const uint8_t*)ads_config_buffer, sizeof(ads_config_buffer), NULL);
	//store in flash
	memset(local_buffer, 0xFF, sizeof(local_buffer));
	memcpy(&local_buffer[0], ads_config_buffer, sizeof(ads_config_buffer));
	memcpy(&local_buffer[sizeof(ads_config_buffer)], &crc, sizeof(crc));

	for(uint i = 0; i < (LEN_BUFFER/LEN_WRITE_OP); i++)
	{
		st = mem_write(addr, &local_buffer[i*LEN_WRITE_OP], LEN_WRITE_OP);
		addr += LEN_WRITE_OP;
		if(MEMST_OK != st)
		{
			_error("failed to write ads config into flash");
			break;
		}
		util_blocking_delay_ms(20);
	}
	return APPST_SUCCESS;
}

static void read_ads_config_from_flash(void)
{
	memory_status_t st;
	uint16_t crc_r;
	uint16_t ref_res = 1000;

	memset(ads_config_buffer, 0, sizeof(ads_config_buffer));
	memset(local_buffer, 0, sizeof(local_buffer));
	st = mem_read(ADS_CONFIG_ADDR, local_buffer, sizeof(local_buffer));
	if(MEMST_OK != st)
	{
		memset(local_buffer, 0, sizeof(local_buffer));
		_error("failed to read ads config from flash %d", st);
		return;
	}
	memcpy(ads_config_buffer, &local_buffer[0], sizeof(ads_config_buffer));
	//check data integrity
	memcpy(&crc_r, &local_buffer[sizeof(ads_config_buffer)], sizeof(crc_r));

	crc = crc16_compute((const uint8_t*)ads_config_buffer, sizeof(ads_config_buffer), NULL);
	//_debug("crc = %d crc_r = %d",crc,crc_r);	//For debugging purpose
	if(crc != crc_r)
	{
		memset(ads_config_buffer, 0, sizeof(ads_config_buffer));
		memcpy(&ads_config_buffer[0], default_conf, sizeof(default_conf));
		memcpy(&ads_config_buffer[5], &ref_res, sizeof(uint16_t));
		_info("Checksum failed");
	}

	_info("finished loading ads config from flash");
}

