/*
 * tps65987.c
 *
 *  Created on: Dec 19, 2019
 *      Author: Myant
 */

#include <string.h>
#include "tps65987.h"
#include "../diagnostic.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "mya_util.h"
#include "tskctrl.h"
#include "logger.h"
#include "patch_data.h"

#if DEVKIT_NRF52 == 1
#define PMIC_I2C_ADDR       0x38  //dev kit i2c address
#else
#define PMIC_I2C_ADDR       0x21
#endif //DEVKIT_NRF52

//20us to send 1 byte at 400KHz +  1b address + 1b tolerance
#define TIME_XFER(NBYTES)   (1500*(NBYTES+2))

//configuration data
//not shipping mode, no interrupt for bat_fault and charger_fault
#define DATA_MISC_CNT               0x48  //config data
#define BATFET_DISABLE              1<<5
#define EN_HIZ                      1<<7
#define DISABLE_HIZ                 0x7F
#define VIN_LIMIT                   0x0B<<3  //input voltage limit 4.76V
#define IIN_LIMIT                   0x5      //1.5A
#define THERMAL_LIMIT               0x0      //60C
#define ITERM                       0x2      //termination current: 256mA(0.1C)
#define IPRECHG                     0x2<<4   //Pre-Charge Current : 256mA(0.1C)
#define OTG_DISABLE                (~(1<<5))  //disable OTG

//registers addresses
#define REG_MODE                        0x03
#define REG_TX_SINK_CAP                 0x33
#define REG_CMD1                        0x08
#define REG_DATA1                       0x09
#define REG_IntEvent1                   0x14
#define REG_IntEvent2                   0x15
#define REG_IntMask1                    0x16
#define REG_IntMask2                    0x17
#define REG_IntClear1                   0x18
#define REG_IntClear2                   0x19
#define REG_AUTO_NEGOTIATE_SINK         0x37
#define REG_GLOBAL_SYSTEM_CONF          0x27
#define REG_POWER_PATH_STATUS           0x26
#define REG_RX_SOURCE_CAPS              0x30
#define REG_BATTERY_STATUS_INFO         0x7A
#define REG_BATTERY_CAPABILITY_INFO     0x7C
#define REG_PD3_CONFIGURATION_REGISTER  0x42
#define REG_PD3_STATUS_REGISTER  		0x41

#define CMD_ANeg                        0x67654e41  //auto negotiate contract
#define CMD_Gaid                        0x64696147  //warm reset
#define CMD_GSrC                        0x43725347  //get source capability
#define CMD_PTCs                        0x73435450
#define CMD_PTCr                        0x72435450
#define CMD_PTCd                        0x64435450
#define CMD_PTCc                        0x63435450
#define CMD_GBaS                        0x53614247  //battery status
#define CMD_GBaC                        0x43614247  //battery capability 

#define STD_TAKS_WAIT  5 //in ms
#define BUF_LEN 100

#define VOLT_NUM 10  //maximum battery voltage number availible
#define TOTAL_BATTERIES    8   // Total number of batteries (4 fixed + 4 hot-swappable)

static bool config_i2c(void);
static bool write_register(uint8_t reg, uint8_t * data, uint16_t len, bool wait);
static bool read_register(uint8_t reg, uint8_t *read_buf, uint16_t len);
static bool download_patch(void);
static void irq_i2c(I2CStatus evt);
static pmic_status_t execute_task(uint32_t command, uint8_t *input, uint8_t len);
static pmic_status_t execute_std_task(uint32_t command, uint8_t *input, uint8_t len);
static pmic_status_t read_tx_sink_capabilities(uint8_t *data);
static pmic_status_t write_tx_sink_capabilities(void)  __attribute__((unused));
static pmic_status_t read_auto_negotiate_sink(uint8_t *data);
static pmic_status_t write_auto_negotiate_sink(void);
static pmic_status_t read_global_system_conf(uint8_t *data);
static pmic_status_t read_power_path_status(uint8_t *data);
static pmic_status_t read_rx_source_cap(uint8_t *data);
static uint8_t get_max_voltage(uint8_t *data,uint8_t len);

#ifdef Battery_SOC_Test
static void read_battery_socs();
static pmic_status_t read_battery_status(uint8_t *data);
static uint16_t extract_battery_soc(uint8_t *data, uint8_t start_index);
static uint8_t previous_soc = 0;	// variable to store previous SOC values for change detection
static uint8_t battery_soc[TOTAL_BATTERIES]; // Array to store SOC of all batteries
#endif

static uint8_t cmd_tx[2] = {0};
static bool txend = false;

static uint8_t volt_num=0;
static uint8_t voltage_level[VOLT_NUM]={0};  //voltage in unit of 100mv
static uint8_t voltage_max=0;
static uint8_t voltage_settings_pdo2[7][4]={{0}};

//PDO1 is always fixed 5v
static  uint8_t voltage_settings_pdo1[4]={0x5a, 0x90,  0x01,0x36};
static uint8_t voltage_settings_pdo1_ext[4]={0x2c, 0x69, 0x01,0x40 };
static uint8_t voltage_settings_pdo2_ext[4]={0x2c, 0x69, 0x01,0x40 };
static uint8_t tx_sink_cap[57]={0};
static uint8_t auto_neg_sink_reg[20]={0x6f, 0x0d, 0x48 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0xc8, 0x68, 0x01 ,0x00 ,0x00 , 0x00,  0x00, 0x00,  0x2c, 0x1, 0x00,0x00 };
//used in functions only run in initialization stage, declared as static to reduce stack needs
static uint8_t init_buf[BUF_LEN]={0xff};  //used in tps_init()
static uint8_t patch_buf[BUF_LEN]={0xff}; //used in download_patch()
static uint8_t exec_buf[0x40]; //used in executive functions
//used in functions to deal with i2c access,must be finished to proceed, declared as static to reduce stack needs
static uint8_t reg_buf[BUF_LEN+1]={0xff};//used in read_register() and write_register()
#if PCB_ID == PCB_MULTI_CHANNEL
static uint8_t nego_buf[BUF_LEN]={0xff}; //used in tps_neg_contract()

#ifdef Command_Test
// Packed structure to match the exact bit layout of the 20-byte register
typedef struct __attribute__((packed)) {
    // Byte 1: Auto-negotiate control and RDO flags
    struct {
        uint8_t AutoNgt : 1;                 // Bit 0: Auto Negotiate Fixed PDO
        uint8_t AutoNgtSnkBattery : 1;       // Bit 1: Auto Negotiate using Battery PDO
        uint8_t AutoNgtSnkVariable : 1;      // Bit 2: Auto Negotiate using Variable PDO
        uint8_t RDOUsbCommCapable : 1;       // Bit 3: RDO USB Communications Capable Flag
        uint8_t OfferPriority : 2;           // Bits 4-5: Offer Priority
        uint8_t RDONoUsbSuspFlag : 1;        // Bit 6: RDO NoUSBSusp Flag
        uint8_t RDOGiveBackFlag : 1;         // Bit 7: RDO GiveBack Flag
    } auto_negotiate_control;

    // Byte 2: Auto-negotiate control
    struct {
        uint8_t AutoComputeSinkMinPower : 1; // Bit 0: Auto Compute Sink Min Power
        uint8_t reserved : 7;                // Bits 1-7: Reserved
    } auto_compute_control;

    // Bytes 3-4: Auto Negotiate Minimum Sink Required Operating Power
    struct {
        uint16_t ANSinkMinRequiredPower : 10;// Bits 0-9: Minimum operating power
        uint16_t reserved : 6;               // Bits 10-15: Reserved
    } min_sink_power;

    // Bytes 5-8: RDO Power Parameters
    struct {
        uint32_t OperatingPower : 10;        // Bits 0-9: Operating Power (250 mW steps)
        uint32_t MinOperatingPower : 10;     // Bits 10-19: Minimum Operating Power (250 mW steps)
        uint32_t reserved : 12;              // Bits 20-31: Reserved
    } rdo_power_params;

    // Bytes 9-12: RDO Current Parameters
    struct {
        uint32_t OperatingCurrent : 10;      // Bits 0-9: Operating Current (10 mA steps)
        uint32_t MinOperatingCurrent : 10;   // Bits 10-19: Minimum Operating Current (10 mA steps)
        uint32_t reserved : 12;              // Bits 20-31: Reserved
    } rdo_current_params;

    // Bytes 13-16: Battery PDO Parameters
    struct {
        uint32_t MaximumPower : 10;          // Bits 0-9: Maximum Power (250 mW steps)
        uint32_t MaximumVoltage : 10;        // Bits 10-19: Maximum Voltage (50 mV steps)
        uint32_t reserved1 : 2;              // Bits 20-21: Reserved
        uint32_t MinimumVoltage : 10;        // Bits 22-31: Minimum Voltage (50 mV steps)
    } battery_pdo_params;

    // Bytes 17-20: Non-Battery PDO Parameters
    struct {
        uint32_t MaximumCurrent : 10;        // Bits 0-9: Maximum Current (10 mA steps)
        uint32_t reserved1 : 10;             // Bits 10-19: Reserved
        uint32_t PeakCurrent : 2;            // Bits 20-21: Peak Current
        uint32_t reserved2 : 10;             // Bits 22-31: Reserved
    } non_battery_pdo_params;
} UsbPdNegotiationParams;

// PD3.0 Configuration Register Bitfield Structure
// Follows the bit definitions from Table 3-42
typedef struct {
    // Byte 1-4 (32-bit) bitfield definition
    uint32_t sop_revision : 2;            // Bits 0-1: SOP Revision
    uint32_t sop_prime_revision : 2;      // Bits 2-3: SOP Prime Revision
    uint32_t unchunked_supported : 1;     // Bit 4: Unchunked messages support
    uint32_t fr_swap_enabled : 1;         // Bit 5: Fast Role Swap enabled
    uint32_t fr_signal_disabled_for_uvp : 1; // Bit 6: Fast Role Swap signaling configuration
    uint32_t reserved_7 : 1;              // Bit 7: Reserved

    uint32_t frs_swap_init_timer : 4;     // Bits 8-11: FRS initiation timer
    uint32_t reserved_12 : 1;             // Bit 12: Reserved
    uint32_t reserved_15_13 : 3;          // Bits 13-15: Reserved

    uint32_t support_source_cap_ext_msg : 1;  // Bit 16: Source Capabilities Extended messages support
    uint32_t support_status_msg : 1;          // Bit 17: Status messages support
    uint32_t support_battery_cap_msg : 1;     // Bit 18: Battery Capabilities messages support
    uint32_t support_battery_status_msg : 1;  // Bit 19: Battery Status messages support
    uint32_t support_manufacture_info_msg : 1;// Bit 20: Manufacturer Info messages support
    uint32_t support_security_msg : 1;        // Bit 21: Security messages support
    uint32_t support_firmware_upgrade_msg : 1;// Bit 22: Firmware Upgrade messages support
    uint32_t support_pps_status_msg : 1;      // Bit 23: PPS Status messages support
    uint32_t support_country_code_info : 1;   // Bit 24: Country Code messages support

    uint32_t reserved_31_25 : 7;          // Bits 25-31: Reserved
} __attribute__((packed)) PD30ConfigRegister;

// Union to allow safe type conversion
typedef union {
    PD30ConfigRegister bitfields;
    uint8_t bytes[4];
    uint32_t raw_value;
} PD30ConfigRegisterUnion;

// PD3.0 Status Register Bitfield Structure
// Follows the bit definitions from Table 3-40
typedef struct {
    // Byte 1-4 (32-bit) bitfield definition
    uint32_t not_supported_rcvd : 1;          // Bit 0: Not Supported Message received
    uint32_t src_cap_ext_rcvd : 1;            // Bit 1: Source Capabilities Extended Message received
    uint32_t sec_rsp_rcvd : 1;                // Bit 2: Security Response Message received
    uint32_t sec_req_rcvd : 1;                // Bit 3: Security Request Message received

    uint32_t reserved_22_5 : 18;              // Bits 4-22: Reserved

    uint32_t port_partner_neg_spec_svdm_rev : 2;  // Bits 23-24: Port Partner Negotiated SVDM Spec Revision
    uint32_t plug_partner_neg_spec_svdm_rev : 2;  // Bits 25-26: Plug Partner Negotiated SVDM Spec Revision
    uint32_t port_partner_neg_spec_rev : 2;   // Bits 27-28: Port Partner Negotiated Spec Revision
    uint32_t plug_partner_neg_spec_rev : 2;   // Bits 29-30: Plug Partner Negotiated Spec Revision
    uint32_t use_unchunked_messages : 1;      // Bit 31: Unchunked messages supported
} __attribute__((packed)) PD30StatusRegister;

// Power Path Status Register Bitfield Structure
typedef struct {
    // Byte 1 (Least Significant Byte): PP1/PP2 CABLE switch and power
    uint8_t pp1_cable_switch : 2;        // Bits 0-1: PP1_CABLE switch state
    uint8_t pp2_cable_switch : 2;        // Bits 2-3: PP2_CABLE switch state
    uint8_t reserved_5_4 : 2;            // Bits 4-5: Reserved
    uint8_t pp1_cable_enabled : 1;       // Bit 0: PP1_CABLE Power State
    uint8_t pp2_cable_enabled : 1;       // Bit 1: PP2_CABLE Power State

    // Byte 2: PP1, PP2 switch states
    uint8_t pp1_switch : 3;              // Bits 6-8: PP1 switch state
    uint8_t pp2_switch : 3;              // Bits 9-11: PP2 switch state
    uint8_t pp3_switch : 3;              // Bits 12-14: PP3 switch state
    uint8_t pp4_switch : 3;              // Bits 15-17: PP4 switch state

    // Byte 3: Overcurrent and Power Source
    uint8_t pp1_overcurrent : 1;         // Bit 4: PP1 Overcurrent
    uint8_t pp2_overcurrent : 1;         // Bit 5: PP2 Overcurrent
    uint8_t pp1_cable_overcurrent : 1;   // Bit 10: PP1_CABLE Overcurrent
    uint8_t pp2_cable_overcurrent : 1;   // Bit 11: PP2_CABLE Overcurrent
    uint8_t reserved_12_13 : 2;          // Bits 12-13: Reserved
    uint8_t power_source : 2;            // Bits 14-15: Power Source

    // Bytes 4-6: Reverse Current Protection and Reserved
    uint8_t pp1_rcp : 1;                 // Bit 16: PP1 Reverse Current Protection
    uint8_t pp2_rcp : 1;                 // Bit 17: PP2 Reverse Current Protection
    uint32_t reserved_18_31 : 14;        // Bits 18-31: Reserved
} __attribute__((packed)) PowerPathStatusRegister;

// Union to allow safe type conversion
typedef union {
    PowerPathStatusRegister bitfields;
    uint8_t bytes[8];
    uint64_t raw_value;
} PowerPathStatusRegisterUnion;

typedef struct __attribute__((packed)) {
    uint32_t TypeCCurrent : 2;
    uint32_t DisablePD : 2;
    uint32_t ProcessSwapToSink : 1;
    uint32_t InitiateSwapToSink : 1;
    uint32_t ProcessSwapToSource : 1;
    uint32_t InitiateSwapToSource : 2;
    uint32_t ProcessVconnSwap : 1;
    uint32_t Reserved1 : 2;
    uint32_t ProcessSwapToUFP : 1;
    uint32_t InitiateSwapToUFP : 1;
    uint32_t ProcessSwapToDFP : 1;
    uint32_t InitiateSwapToDFP : 1;
    uint32_t AutomaticIDRequest : 1;
    uint32_t ForceUSB3Gen1 : 1;
    uint32_t Reserved2 : 1;
    uint32_t ExternallyPowered : 1;
    uint32_t AutomaticSinkCapRequest : 1;
    uint32_t SinkControlBit : 1;
    uint32_t Reserved3 : 2;
    uint32_t Resistor15kPresent : 1;
    uint32_t DCDEnable : 1;
    uint32_t ChargerAdvertiseEnable : 3;
    uint32_t USBDisable : 1;
    uint32_t ChargerDetectEnable : 2;
} PortControlBitfields;

typedef union {
    uint32_t raw;
    uint8_t bytes[4];
    PortControlBitfields bits;
} PortControlRegister;

typedef struct __attribute__((packed)) {
    uint64_t TypeCStateMachine : 2;
    uint64_t Reserved1 : 1;
    uint64_t ReceptacleType : 3;
    uint64_t AudioAccessorySupport : 1;
    uint64_t DebugAccessorySupport : 1;
    uint64_t SupportTypeCOptions : 2;
    uint64_t Reserved2 : 1;
    uint64_t VCONNsupported : 2;
    uint64_t USB3rate : 2;
    uint64_t Reserved3 : 1;
    uint64_t VBUS_SetUvpTo4P5V : 1;
    uint64_t VBUS_UvpTripPoint5V : 3;
    uint64_t VBUS_UvpTripHV : 3;
    uint64_t VBUS_OvpTripPoint : 6;
    uint64_t VBUS_OvpUsage : 2;
    uint64_t VBUS_HighVoltageWarningLevel : 1;
    uint64_t VBUS_LowVoltageWarningLevel : 1;
    uint64_t SoftStart : 2;
    uint64_t Reserved4 : 1;
    uint64_t EnableUVPDebounce : 1;
    uint64_t Reserved5 : 3;
    uint64_t PowerThresAsSourceContract : 8;
    uint64_t VoltageThresAsSinkContract : 8;
    uint64_t Reserved6 : 8;
} PortConfigurationRegister;

typedef union {
    uint64_t raw;
    uint8_t bytes[8];
    PortConfigurationRegister bits;
} PortConfigurationUnion;

typedef struct __attribute__((packed)) {
    uint32_t PlugDetails : 2;
    uint32_t CCPullUp : 2;
    uint32_t PortType : 2;
    uint32_t PresentRole : 1;
    uint32_t Reserved1 : 1;
    uint32_t SoftResetType : 5;
    uint32_t Reserved2 : 3;
    uint32_t HardResetDetails : 6;
    uint32_t Reserved3 : 10;
} PDStatusRegister;

typedef union {
    uint32_t raw;
    uint8_t bytes[4];
    PDStatusRegister bits;
} PDStatusRegisterUnion;


// Packed structure to represent the 0x37 Auto Negotiate Sink Register
typedef struct __attribute__((packed)) {
    // Byte 1: Auto-negotiate control and RDO flags
    uint8_t autoNgt : 1;                // Bit 0 Auto Negotiate Fixed PDO
    uint8_t autoNgtSnkBattery : 1;      // Bit 1 Auto Negotiate using Battery PDO
    uint8_t autoNgtSnkVariable : 1;     // Bit 2 Auto Negotiate using Variable PDO
    uint8_t rdoUsbCommCapable : 1;      // Bit 3 RDO USB Communications Capable Flag
    uint8_t offerPriority : 2;          // Bits 5:4 Offer Priority
    uint8_t rdoNoUsbSuspFlag : 1;       // Bit 6 RDO NoUSB Susp Flag
    uint8_t rdoGiveBackFlag : 1;        // Bit 7 RDO GiveBack Flag

    // Byte 2: Auto-negotiate control
    uint8_t autoComputeSinkMinPower : 1; // Bit 0 Auto compute sink min power
    uint8_t reserved7 : 7;             // Bits 7:1 Reserved (Write 0)

    // Bytes 3-4: Auto Negotiate Minimum Sink Required Operating Power
    uint16_t anSinkMinRequiredPower : 10; // Bits 9:0 Minimum operating power (250mW per LSB)
    uint16_t reserved6 : 6;            // Bits 15:10 Reserved (Write 0)

    // Bytes 5-8: RDO Power Parameters
    uint32_t operatingPower : 10;      // Bits 9:0 Operating Power (250 mW steps)
    uint32_t minOperatingPower : 10;   // Bits 19:10 Min Operating Power (250 mW steps)
    uint32_t reserved5 : 12;           // Bits 31:20 Reserved (Write 0)

    // Bytes 9-12: RDO Current Parameters
    uint32_t operatingCurrent : 10;    // Bits 9:0 Operating Current (10 mA steps)
    uint32_t minOperatingCurrent : 10; // Bits 19:10 Min Operating Current (10 mA steps)
    uint32_t reserved4 : 12;           // Bits 31:20 Reserved (Write 0)

    // Bytes 13-16: Battery PDO Parameters
    uint32_t maximumPower : 10;        // Bits 9:0 Maximum Power (250 mW steps)
    uint32_t minimumVoltage : 10;      // Bits 31:22 Minimum Voltage (50 mV steps)
    uint32_t maximumVoltage : 10;      // Bits 19:10 Maximum Voltage (50 mV steps)
    uint32_t reserved3 : 2;            // Bits 21:20 Reserved (Write 0)

    // Bytes 17-20: Non-Battery PDO Parameters
    uint32_t maximumCurrent : 10;      // Bits 9:0 Maximum Current (10 mA steps)
    uint32_t reserved2 : 10;           // Bits 31:22 Reserved
    uint32_t peakCurrent : 2;          // Bits 21:20 Peak Current
    uint32_t reserved1 : 10;           // Bits 9:0 Reserved
} AutoNegotiateSinkRegister;
#endif

#endif

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


pmic_status_t tps_init(void)
{
    bool ret, patch_laoded=false;
//    hal_gpio_set(PD_RESET);
//    util_blocking_delay_ms(10);
//    hal_gpio_clr(PD_RESET);
//    util_blocking_delay_ms(10);

    ret = config_i2c();
    if(!ret)
    	return PMICST_I2C_ERROR;
    /*check mode register to see if there is any issues the first byte is how
     * many bytes that will be following,eg,there are 4 bytes for mode, so the
     * first byte's value will be 4, and total 5 bytes will be read*/
    while(1)
    {
    	if(!read_register(REG_MODE, init_buf,4))
		{
			log_error("[tps_init] error mode read" );
			util_blocking_delay_ms(50);
		}
    	else
    		break;
    }
    if(init_buf[0]=='P'&&init_buf[1]=='T'&&init_buf[2]=='C'&&init_buf[3]=='H')
    {
    	log_info("start to download patch");
		if(download_patch())
		{
			log_info("finished download patch");
			patch_laoded=true;
		}
	}
	if(!read_register(REG_MODE, init_buf,4))
    {
    	log_error("[tps_init] error mode read" );
    	return PMICST_I2C_ERROR;
    }
    else
		log_info("[tps_init] mode=%c%c%c%c",init_buf[0],init_buf[1],init_buf[2],init_buf[3]);
	if(init_buf[0]=='A'&&init_buf[1]=='P'&&init_buf[2]=='P')
    {
    	if(PMICST_OK==read_tx_sink_capabilities(tx_sink_cap))
		{
			log_info("[tps_init] tx_sink_org=0x%x",tx_sink_cap[0]);
			if(!patch_laoded)
			{
				for(uint8_t i=0;i<4;i++)
				{
					tx_sink_cap[i+1]=voltage_settings_pdo1[i];
				}
				for(uint8_t i=0;i<4;i++)
				{
					tx_sink_cap[4*7+i+1]=voltage_settings_pdo1_ext[i];
				}
				tx_sink_cap[0]=1;
			}
			for(uint8_t i=0;i<4;i++)
			{
				tx_sink_cap[4*8+i+1]=voltage_settings_pdo2_ext[i];
			}
		}
		else
		{
			log_error("[tps_init] error tx_sink_cap read" );
			return PMICST_I2C_ERROR;
		}
		if(PMICST_OK!=read_global_system_conf(init_buf))
		{
			log_error("[tps_init] error read_global_system_conf" );
			return PMICST_I2C_ERROR;
		}
		if(PMICST_OK!=read_power_path_status(init_buf))
		{
			log_error("[tps_init] error read_power_path_status " );
			return PMICST_I2C_ERROR;
		}
#ifdef Command_Test
	    // Create a union instance
		PowerPathStatusRegisterUnion* status_union = (PowerPathStatusRegisterUnion*)init_buf;

		read_register(REG_PD3_STATUS_REGISTER, init_buf,4);
		PD30StatusRegister* config_ptr_4 = (PD30StatusRegister*)init_buf;
#endif
		//send get source command to get source caps
		while(1) //the battery bank may be not ready
		{
			if(execute_std_task(CMD_GSrC, NULL, 0)!=PMICST_OK)
			{
				log_error("[tps] error 4cc CMD_GSrC ");
				util_blocking_delay_ms(100);
			}
			else
				break;
		}

		if(PMICST_OK!=read_rx_source_cap(init_buf))
		{
			log_error("[tps_init] error read_rx_source_cap " );
			return PMICST_I2C_ERROR;
		}
		else
		{
			init_buf[0]&=0x07;
			log_info("[tps_init] num of rx_source_cap=0x%x",init_buf[0]);
			volt_num=init_buf[0];
			if(volt_num>0&&volt_num<=VOLT_NUM)
			{
				for(uint8_t i=0;i<init_buf[0];i++)
				{
					//calculate the voltage supported in unit of 100mv
					voltage_level[i]=  ((init_buf[i*4+3]&0xf)<<5)  +  (init_buf[i*4+2]>>3)  ;
					voltage_max=get_max_voltage(voltage_level, volt_num);
					for(uint8_t n=0;n<4;n++)
					{
						voltage_settings_pdo2[i][n]=init_buf[i*4+1+n];
					}
				}
			}
			else
			{
				log_error("[tps_init] did not get source caps" );
				return PMICST_I2C_ERROR;
			}
		}

		if(PMICST_OK!=read_auto_negotiate_sink(init_buf))
		{
			log_error("[tps_init] error read_auto_negotiate_sink " );
			return PMICST_I2C_ERROR;
		}

#ifdef Command_Test

		AutoNegotiateSinkRegister* auto_neg_sink = (AutoNegotiateSinkRegister*)init_buf;

		read_register(0x28, init_buf, 8);
		PortConfigurationUnion* port_config = (PortConfigurationUnion*)init_buf;

		read_register(0x29, init_buf, 4);
		PortControlRegister* port_control = (PortControlRegister*)init_buf;

		read_register(0x40, init_buf, 4);
		PDStatusRegisterUnion* pd_status = (PDStatusRegisterUnion*)init_buf;

		AutoNegotiateSinkRegister* auto_neg_sink_write = (AutoNegotiateSinkRegister*)auto_neg_sink_reg;
#endif

		if(PMICST_OK!=write_auto_negotiate_sink())
		{
			log_debug("[tps_init] error write_auto_negotiate_sink" );
			return PMICST_I2C_ERROR;
		}

#ifdef Command_Test

		read_register(REG_PD3_CONFIGURATION_REGISTER, init_buf,4);
		PD30ConfigRegister* config_ptr = (PD30ConfigRegister*)init_buf;

		uint8_t cmd_param = 0x00;
		while(1) //the battery bank may be not ready
		{
			if(execute_std_task(CMD_GBaS, &cmd_param, 1) != PMICST_OK)
			{
				log_error("[tps] error 4cc CMD_GBaS");
				util_blocking_delay_ms(1000);
			}
			else
				break;
		}

		read_register(REG_PD3_STATUS_REGISTER, init_buf,4);
		PD30StatusRegister* config_ptr_2 = (PD30StatusRegister*)init_buf;

	    // Create a union instance
	    PD30ConfigRegisterUnion config_union;

	    // Initialize all bits to 0
	    config_union.raw_value = 0;

	    // Set specific configuration bits
	    config_union.bitfields.sop_revision = 0b10;  // Revision 3.0
	    config_union.bitfields.sop_prime_revision = 0b10;  // Revision 3.0
	    config_union.bitfields.unchunked_supported = 1;
	    config_union.bitfields.support_battery_cap_msg = 1;
	    config_union.bitfields.support_battery_status_msg = 1;
	    config_union.bitfields.support_source_cap_ext_msg = 1;

	    // Access as array of bytes
	    uint8_t* bytes_array = config_union.bytes;

	    // Access as raw 32-bit value
	    uint32_t raw_value = config_union.raw_value;

	    //the first byte read is the numbers of data that will be followed
	    write_register(REG_PD3_CONFIGURATION_REGISTER, bytes_array,4,true);

		read_register(REG_PD3_CONFIGURATION_REGISTER, init_buf,4);
		PD30ConfigRegister* config_ptr_1 = (PD30ConfigRegister*)init_buf;

		read_register(REG_PD3_STATUS_REGISTER, init_buf,4);
		PD30StatusRegister* config_ptr_3 = (PD30StatusRegister*)init_buf;
#endif

    }
    else
    	return PMICST_I2C_ERROR;
    return PMICST_OK;
}

bool tps_get_available_voltages(uint8_t *voltages, uint8_t *n_voltages)
{
	if(NULL == n_voltages || NULL == voltages)
		return false;
	if(*n_voltages < volt_num)
		return false;

	*n_voltages = volt_num;
	memcpy(voltages, voltage_level, volt_num);
	return true;
}

bool tps_voltagelevel_valid(uint8_t level)
{
#if PCB_ID == PCB_DUAL_CHANNEL
	return true;
#elif PCB_ID == PCB_MULTI_CHANNEL
	if(level==0)
		return false;
	else
	{
		for(uint8_t i=0;i<volt_num;i++)
		{
			if(voltage_level[i]==level)
				return true;
		}
	}
	return false;
#endif
}

//get back voltage caps from the unit of sel
uint8_t tps_retrieve_voltagelevel(uint8_t sel)
{
	if(sel>=volt_num)
	{
		return 0;
	}
	return voltage_level[sel];
}

uint8_t tps_retrieve_max_volt(void)
{
#if PCB_ID == PCB_DUAL_CHANNEL
	return 120;
#elif PCB_ID == PCB_MULTI_CHANNEL
	return voltage_max;
#endif
}

static bool write_register(uint8_t reg, uint8_t * data, uint16_t len, bool wait)
{
	I2CStatus status;
	bool txok;
	uint8_t *buf_pointer=reg_buf;
	if(len>BUF_LEN-1)
	    return false;
	*buf_pointer++ = reg;
	*buf_pointer++ = len;
	memcpy(buf_pointer,data,len);

    txend = false;
    txok = true;
    hal_i2c_set_irq_callback(irq_i2c);
    status = hal_i2c_tx(PMIC_I2C_ADDR, reg_buf, 2+len, true);
    if((I2CST_OP_OK == status) && wait)
        txok = util_check_flag_us(&txend, TIME_XFER(len+2));
    if((txok||txend) && (I2CST_OP_OK == status))
    	return true;
    else
    {
    	diag_inc_flag(FLG_PMIC_WRITE);
    	return false;
    }
}

static bool read_register(uint8_t reg, uint8_t *read_buf, uint16_t len)
{
    I2CStatus status;
    bool txok;
    uint8_t *buf_pointer=reg_buf;
    cmd_tx[0] = reg;
    txok = true;
    txend = false;
    if(len>BUF_LEN-1)
    {
       	log_error("data length exceed range");
    	return false;
    }

    hal_i2c_set_irq_callback(irq_i2c);
    //false to indicate not producing stop signal so that re-start signal will
    //be produced for consequent read
    status = hal_i2c_tx(PMIC_I2C_ADDR, cmd_tx, 1, false);
    if(I2CST_OP_OK == status)
    {
        txok = util_check_flag_us(&txend, TIME_XFER(1));
        if(txok||txend)
        {
            txok = false;
            txend = false;
            status = hal_i2c_rx(PMIC_I2C_ADDR, reg_buf, len+1);
            if(I2CST_OP_OK == status)
                txok = util_check_flag_us(&txend, TIME_XFER(len));
            else
			{
				log_error("i2c read error 1");
				diag_inc_flag(FLG_PMIC_READ);
				return false;
			}
        }
    }
    else
    {
    	log_error("i2c write error");
    	diag_inc_flag(FLG_PMIC_WRITE);
    	return false;
    }
    if(reg_buf[0]!=len)
    {
    	log_error("wrong read number len= %x, reg: %d", reg_buf[0], reg);
    	diag_inc_flag(FLG_PMIC_READ_DATA);
    	return false;
    }
    memcpy(read_buf,(buf_pointer+1),len);
    return (txend && (I2CST_OP_OK == status));
}



static void irq_i2c(I2CStatus evt)
{
    if(I2CST_IRQ_OP_DONE == evt)
    {
        txend = true;
    }
    else if(I2CST_IRQ_ADDR_NACK == evt)
    {
    	diag_inc_flag(FLG_I2C_IRQ_PMIC);
        log_error("[tps] i2c addr error: %d", evt);
    }
    else if(I2CST_IRQ_DATA_NACK == evt)
    {
    	diag_inc_flag(FLG_I2C_IRQ_PMIC);
		log_error("[tps] i2c data error: %d", evt);
    }
}

pmic_status_t read_tx_sink_capabilities(uint8_t *data)
{
    bool ret;

    //the first byte read is the numbers of data that will be followed
    ret=read_register(REG_TX_SINK_CAP, data,sizeof(tx_sink_cap));
    if(!ret)
      	return PMICST_I2C_ERROR;
    else
    	return PMICST_OK;
}

pmic_status_t read_auto_negotiate_sink(uint8_t *data)
{
    bool ret;

    //the first byte read is the numbers of data that will be followed
    ret=read_register(REG_AUTO_NEGOTIATE_SINK, data,sizeof(auto_neg_sink_reg));
    if(!ret)
      	return PMICST_I2C_ERROR;
    else
    	return PMICST_OK;
}

pmic_status_t write_auto_negotiate_sink(void)
{
    bool ret;

    //the first byte read is the numbers of data that will be followed
    ret=write_register(REG_AUTO_NEGOTIATE_SINK, auto_neg_sink_reg,sizeof(auto_neg_sink_reg),true);
    if(!ret)
      	return PMICST_I2C_ERROR;
    else
    	return PMICST_OK;
}

pmic_status_t read_global_system_conf(uint8_t *data)
{
    bool ret;

    //the first byte read is the numbers of data that will be followed
    ret=read_register(REG_GLOBAL_SYSTEM_CONF, data,14);
    if(!ret)
      	return PMICST_I2C_ERROR;
    else
    	return PMICST_OK;
}

pmic_status_t read_power_path_status(uint8_t *data)
{
    bool ret;

    //the first byte read is the numbers of data that will be followed
    ret=read_register(REG_POWER_PATH_STATUS, data,8);
    if(!ret)
      	return PMICST_I2C_ERROR;
    else
    	return PMICST_OK;
}

pmic_status_t read_rx_source_cap(uint8_t *data)
{
    bool ret;

    //the first byte read is the numbers of data that will be followed
    ret=read_register(REG_RX_SOURCE_CAPS, data,29);
    if(!ret)
      	return PMICST_I2C_ERROR;
    else
    	return PMICST_OK;
}

pmic_status_t write_tx_sink_capabilities(void)
{
    bool ret;

    ret=write_register(REG_TX_SINK_CAP, tx_sink_cap,sizeof(tx_sink_cap), true);
    if(!ret)
      	return PMICST_I2C_ERROR;
    else
    	return PMICST_OK;
}

//none standard task and return data is different
pmic_status_t execute_task(uint32_t command, uint8_t *input, uint8_t len)
{
    bool ret;
    uint8_t cmd[4];

    if(len)
    {
    	if(input==NULL)
    	{
    		log_error("invalid pointer");
    		return PMICST_I2C_ERROR;
    	}

    	else
    	{
    		if(!write_register(REG_DATA1, input,(uint16_t)len, true))
    		{
    			log_error("write REG_DATA1 error");
    			return PMICST_I2C_ERROR;
    		}
    	}
    }

    for(uint8_t i=0;i<4;i++)
    {
    	cmd[i]=(uint8_t)(command>>(i*8));
    }
    ret=write_register(REG_CMD1, cmd,4, true);
    if(!ret)
    {
    	log_error("write REG_CMD1 error");
    	return PMICST_I2C_ERROR;
    }

    while(1)
    {
    	if(read_register(REG_CMD1, cmd, 4))
    	{
    		if(!(cmd[0]||cmd[1]||cmd[2]||cmd[3])) //complited
			{
    			return 	PMICST_OK;
 		    }
    		else if( (cmd[0]=='!')&&(cmd[1]=='C')&&(cmd[2]=='M')&&(cmd[3]=='D') )
    		{
    			log_error("task failed 4");
    			return PMICST_I2C_ERROR;
    		}
    		else
    		{
    			 if(os_is_scheduler_on())
					vTaskDelay(pdMS_TO_TICKS(STD_TAKS_WAIT));//delay 5ms
				 else
					util_blocking_delay_ms(STD_TAKS_WAIT);
    		}
    	}
    	else
    	{
			log_error("task failed 5");
			return PMICST_I2C_ERROR;
		}
    }
    return PMICST_OK;
}

//the standard task return the first byte in Data reg to indicate if it is succsess
static pmic_status_t execute_std_task(uint32_t command, uint8_t *input, uint8_t len)
{
    if(execute_task(command,input, len)!=PMICST_OK)
    {
    	log_error("task failed 1");
    	return PMICST_I2C_ERROR;
    }

    //check if the command success
    if(read_register(REG_DATA1, exec_buf, sizeof(exec_buf)))
	{
		if((exec_buf[0]&0x0f))
		{
			log_error("task failed- %x", exec_buf[0]);
			return PMICST_I2C_ERROR;
		}
		return 	PMICST_OK;
	}
	else
	{
		log_error("task failed 3");
		return PMICST_I2C_ERROR;
	}
}

pmic_status_t execute_patch_reset(void)
{
    uint8_t datax[4]={0x3,0,0xbe,0xef};

    if(execute_task(CMD_PTCr,datax, 4)!=PMICST_OK)
    	return PMICST_I2C_ERROR;
    //check if start success
    if(read_register(REG_DATA1, exec_buf, sizeof(exec_buf)))
	{
		log_info("Patch_reset_st: %x", exec_buf[0]);
	}
	else
		return PMICST_I2C_ERROR;

	return PMICST_OK;
}

pmic_status_t execute_patch_start(void)
{
    uint8_t temp;

    temp=0x3;//DevicePatch and AppConfig included  ????????????
    if(execute_task(CMD_PTCs,&temp, 1)!=PMICST_OK)
    	return PMICST_I2C_ERROR;
    //check if start success
    if(read_register(REG_DATA1, exec_buf, sizeof(exec_buf)))
	{
		log_info("Patch_start_st: %x %x %x", exec_buf[1],exec_buf[2],exec_buf[3]);
	}
	else
		return PMICST_I2C_ERROR;

	return PMICST_OK;
}

pmic_status_t execute_patch_download(uint8_t *input,uint8_t len)
{
    if(execute_task(CMD_PTCd,input, len)!=PMICST_OK)
    	return PMICST_I2C_ERROR;
    //check if start success
    if(read_register(REG_DATA1, exec_buf, sizeof(exec_buf)))
	{
		if(exec_buf[1]!=0)
			return PMICST_I2C_ERROR;
	}
	else
		return PMICST_I2C_ERROR;

	return PMICST_OK;
}

pmic_status_t execute_patch_complete(void)
{
    if(execute_task(CMD_PTCc,NULL, 0)!=PMICST_OK)
    	return PMICST_I2C_ERROR;
    //check if start success
    if(read_register(REG_DATA1, exec_buf, sizeof(exec_buf)))
	{
		log_info("Patch_complete_st: %x %x", exec_buf[2],exec_buf[3]);
	}
	else
		return PMICST_I2C_ERROR;

	return PMICST_OK;
}

#define PATCH_BUNDLE_SIZE 64
static bool download_patch(void)
{
	uint32_t bytesUpdated = 0;
	uint32_t idx = 0;

	while(1)
	{
		if(read_register(REG_IntEvent1, patch_buf, 11))
		{
			if((patch_buf[10]&0x3)==0x2)
			{
				log_info("ready for patch");
				break;
			}
			else
			{
				log_error("Not ready for patch ");
			}

		}
		else
		{
			log_error("read event failed");
		}
		util_blocking_delay_ms(30);
	}

	if(execute_patch_start()!=PMICST_OK)
	{
		log_error("patch not started");
		return false;
	}

	for (idx = 0; idx < gSizeLowregionArray/PATCH_BUNDLE_SIZE; idx++)
	{
		/*
		 * Execute PTCd with PATCH_BUNDLE_SIZE bytes of patch-data
		 * in each iteration
		 */
		if(execute_patch_download((uint8_t *)&tps6598x_lowregion_array[idx * PATCH_BUNDLE_SIZE],PATCH_BUNDLE_SIZE)!=PMICST_OK)
		{
			log_error("patch data download failed idx=%d", idx);
			return false;
		}
		bytesUpdated += PATCH_BUNDLE_SIZE;
	}
	/* Push more bytes if any */
	if(gSizeLowregionArray > bytesUpdated)
	{
		if(execute_patch_download((uint8_t *)&tps6598x_lowregion_array[idx * PATCH_BUNDLE_SIZE],gSizeLowregionArray-bytesUpdated)!=PMICST_OK)
		{
			log_error("patch data download failed idx=%d", idx);
			return false;
		}
	}

	if(execute_patch_complete()!=PMICST_OK)
	{
		log_error("patch not started");
		return false;
	}

	if(read_register(REG_IntClear1, patch_buf, 11))
	{
		log_info("REG_IntClear1[10] =%x", patch_buf[10]);
	}
	patch_buf[10]|=0x3;
	if(!write_register(REG_IntClear1, patch_buf,11,true))
	   log_error("write REG_IntClear1 error ");

	if(read_register(REG_IntEvent1, patch_buf, 11))
	{
		log_info("REG_IntEvent1[10] =%x", patch_buf[10]);
	}

	return true;
}

pmic_status_t tps_neg_contract(uint8_t level)
{
#if PCB_ID == PCB_DUAL_CHANNEL
	return PMICST_OK;
#elif PCB_ID == PCB_MULTI_CHANNEL
    for(uint8_t i=0;i<volt_num;i++)
    {
    	if(level==voltage_level[i])
    	{
    		for(uint8_t n=0;n<4;n++)
			{
				tx_sink_cap[5+n]=voltage_settings_pdo2[i][n];
			}
    		break;
    	}
    	if(i==volt_num-1)
    	{
    		log_error("voltage level not exist");
    		return PMICST_I2C_ERROR;
    	}
    }

    tx_sink_cap[0]=2;
    if(PMICST_OK!=write_tx_sink_capabilities())
    {
    	log_error("[tps] error tx_sink_cap write");
    	return PMICST_I2C_ERROR;
    }
    if(PMICST_OK==read_tx_sink_capabilities(nego_buf))
	{
		log_info("[tps_init] tx_sink_org=0x%x",nego_buf[0]);
	}

    if(execute_std_task(CMD_ANeg, NULL, 0)!=PMICST_OK)
    {
    	log_error("[tps] error 4cc command");
    	return PMICST_I2C_ERROR;
    }

    return PMICST_OK;
#endif
}

#ifdef Battery_SOC_Test
pmic_status_t read_battery_status(uint8_t *data)
{
    bool ret;
    //the first byte read is the numbers of data that will be followed
    ret = read_register(REG_BATTERY_STATUS_INFO, data,32); /* Note: Check the Battery Capability Register Also in future */
    if(!ret)
      	return PMICST_I2C_ERROR;
    else
    	return PMICST_OK;
}

// Function to extract SOC (State of Charge) for a single battery from the BSDO data
uint16_t extract_battery_soc(uint8_t *data, uint8_t start_index) /* Note: Confirm the byte index during testing/debugging: Little/Big Endian */
{
    // SOC is stored in the BatteryPC field (bits 31:16), so we take two bytes starting from `start_index`
	uint16_t soc_value = (data[start_index] << 8) | data[start_index + 1];
	return (uint8_t)(soc_value & 0x00FF); // Truncate to 8-bit value as SOC is in range 0-100
}

void read_battery_socs()
{
    uint8_t data[32];  // Buffer to store 32 bytes read from BSDO register	
	const uint8_t battery_offsets[TOTAL_BATTERIES] = {0, 4, 8, 12, 16, 20, 24, 28}; // Starting byte index for each battery SOC, Battery offsets in BSDO data

	// Corrected Battery offsets in BSDO data to start from the SOC bytes (3rd and 4th byte for each battery)
	// const uint8_t battery_offsets[TOTAL_BATTERIES] = {2, 6, 10, 14, 18, 22, 26, 30}; /* For changed endianness */

	//send get source command to get battery status in BSDO
	while(1) //the battery bank may be not ready
	{
		if(execute_std_task(CMD_GBaS, NULL, 0) != PMICST_OK)
		{
			log_error("[tps] error 4cc CMD_GBaS");
			util_blocking_delay_ms(100);
		}
		else
			break;
	}

    // Read 32 bytes from BSDO register (0x7A) over I2C 
    if (read_battery_status(data) != PMICST_OK) 
	{
        log_error("Error reading BSDO register\n");
        return;
    }

    // Loop over each battery, extract, store its SOC in the soc_array and print its SOC
	for (int i = 0; i < TOTAL_BATTERIES; i++)
	{
        battery_soc[i] = extract_battery_soc(data, battery_offsets[i]);  /* Note: Put this value in the variable or array after verifying the soc results in batt_range function */
        log_info("Battery %d State of Charge (SOC): %d%%\n", i, battery_soc[i]);
    }
}
#endif
/* unit in mv ; will update the code later  */
pmic_status_t tps_read_batt_level(uint16_t * level)
{
	*level = 20000;
	return PMICST_OK;
}

/* unit in percentage; will update the code later  */
pmic_status_t tps_read_batt_range(uint8_t *  range)
{
#ifdef Battery_SOC_Test
	read_battery_socs(); /* Check after how much time this function needs to be computed */
	if (battery_soc[0] != previous_soc) 
	{
		previous_soc = (uint8_t)battery_soc[0]; // Update the previous SOC value to the current SOC
		*range = battery_soc[0]; /* Note: test the value of the SOC */
		os_evt_trigger(EVT_MOD_UPDATE_INFO);
	}
#endif
	*range = 50; /* Note: Remove this after testing */
	return PMICST_OK;
}

static uint8_t get_max_voltage(uint8_t *data,uint8_t len)
{
	uint8_t vol=0;
	if(data==NULL||len==0)
		return 0;
	for(uint8_t i=0; i<len;i++)
	{
		if(data[i]>vol)
			vol=data[i];
	}
	return vol;
}



