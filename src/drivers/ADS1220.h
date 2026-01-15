/* --COPYRIGHT--,BSD
 * Copyright (c) 2018, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/*
 * ADS1220.h
 *
 */
/*******************************************************************************
// ADS1220 Header File for Demo Functions 
//                    
//
// Description: Use of the MSP430F5528 USCI A0 peripheral for setting up and 
//    communicating to the ADS1220 24-bit ADC.
//    
//    
//                                                   
//                 MSP430x552x
//             ------------------                        
//         /|\|                  |                       
//          | |                  |                       
//          --|RST           P3.4|<-- MISO (DOUT)           
//            |                  |                                         
//            |              P3.3|--> MOSI (DIN)
//            |                  |  
//            |              P2.7|--> SCLK
//            |                  | 
//            |              P2.6|<-- INT (DRDY) 
//            |                  | 
//            |              P1.2|--> CS 
//
******************************************************************************/
#ifndef ADS1220_H_
#define ADS1220_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Definition of GPIO Port Bits Used for Communication */
/* P1.2 */
#define ADS1220_CS      	0x04
/* P3.3  */
#define ADS1220_DIN     	0x08
/* P3.4 */
#define ADS1220_DOUT    	0x10
/* P2.6 */
#define ADS1220_DRDY    	0x40
/* P2.7 */
#define ADS1220_SCLK    	0x80
/* Error Return Values */
#define ADS1220_NO_ERROR           0
#define ADS1220_ERROR				-1
/* Command Definitions */
#define ADS1220_CMD_RDATA    	0x10
#define ADS1220_CMD_RREG     	0x20
#define ADS1220_CMD_WREG     	0x40
#define ADS1220_CMD_SYNC    	0x08
#define ADS1220_CMD_SHUTDOWN    0x02
#define ADS1220_CMD_RESET    	0x06
/* ADS1220 Register Definitions */
#define ADS1220_0_REGISTER   	0x00
#define ADS1220_1_REGISTER     	0x01
#define ADS1220_2_REGISTER     	0x02
#define ADS1220_3_REGISTER    	0x03
/* ADS1220 Register 0 Definition */
/*   Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0 
//--------------------------------------------------------------------------------------------
//                     MUX [3:0]                 |             GAIN[2:0]             | PGA_BYPASS
*/
/* Define MUX */
#define ADS1220_MUX_0_1   	0x00
#define ADS1220_MUX_0_2   	0x10
#define ADS1220_MUX_0_3   	0x20
#define ADS1220_MUX_1_2   	0x30
#define ADS1220_MUX_1_3   	0x40
#define ADS1220_MUX_2_3   	0x50
#define ADS1220_MUX_1_0   	0x60
#define ADS1220_MUX_3_2   	0x70
#define ADS1220_MUX_0_G		0x80
#define ADS1220_MUX_1_G   	0x90
#define ADS1220_MUX_2_G   	0xa0
#define ADS1220_MUX_3_G   	0xb0
#define ADS1220_MUX_EX_VREF 0xc0
#define ADS1220_MUX_AVDD   	0xd0
#define ADS1220_MUX_DIV2   	0xe0
/* Define GAIN */
#define ADS1220_GAIN_1      0x00
#define ADS1220_GAIN_2      0x02
#define ADS1220_GAIN_4      0x04
#define ADS1220_GAIN_8      0x06
#define ADS1220_GAIN_16     0x08
#define ADS1220_GAIN_32     0x0a
#define ADS1220_GAIN_64     0x0c
#define ADS1220_GAIN_128    0x0e
/* Define PGA_BYPASS */
#define ADS1220_PGA_BYPASS 	0x01
/* ADS1220 Register 1 Definition */
/*   Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0 
//--------------------------------------------------------------------------------------------
//                DR[2:0]            |      MODE[1:0]        |     CM    |     TS    |    BCS
*/
/* Define DR (data rate) */
#define ADS1220_DR_20		0x00
#define ADS1220_DR_45		0x20
#define ADS1220_DR_90		0x40
#define ADS1220_DR_175		0x60
#define ADS1220_DR_330		0x80
#define ADS1220_DR_600		0xa0
#define ADS1220_DR_1000		0xc0
/* Define MODE of Operation */
#define ADS1220_MODE_NORMAL 0x00
#define ADS1220_MODE_DUTY	0x08
#define ADS1220_MODE_TURBO 	0x10
#define ADS1220_MODE_DCT	0x18
/* Define CM (conversion mode) */
#define ADS1220_CC			0x04
/* Define TS (temperature sensor) */
#define ADS1220_TEMP_SENSOR	0x02
/* Define BCS (burnout current source) */
#define ADS1220_BCS			0x01
/* ADS1220 Register 2 Definition */
/*   Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0 
//--------------------------------------------------------------------------------------------
//         VREF[1:0]     |        50/60[1:0]     |    PSW    |             IDAC[2:0]
*/
/* Define VREF */
#define ADS1220_VREF_INT	0x00
#define ADS1220_VREF_EX_DED	0x40
#define ADS1220_VREF_EX_AIN	0x80
#define ADS1220_VREF_SUPPLY	0xc0
/* Define 50/60 (filter response) */
#define ADS1220_REJECT_OFF	0x00
#define ADS1220_REJECT_BOTH	0x10
#define ADS1220_REJECT_50	0x20
#define ADS1220_REJECT_60	0x30
/* Define PSW (low side power switch) */
#define ADS1220_PSW_SW		0x08
/* Define IDAC (IDAC current) */
#define ADS1220_IDAC_OFF	0x00
#define ADS1220_IDAC_10		0x01
#define ADS1220_IDAC_50		0x02
#define ADS1220_IDAC_100	0x03
#define ADS1220_IDAC_250	0x04
#define ADS1220_IDAC_500	0x05
#define ADS1220_IDAC_1000	0x06
#define ADS1220_IDAC_2000	0x07
/* ADS1220 Register 3 Definition */
/*   Bit 7   |   Bit 6   |   Bit 5   |   Bit 4   |   Bit 3   |   Bit 2   |   Bit 1   |   Bit 0 
//--------------------------------------------------------------------------------------------
//               I1MUX[2:0]          |               I2MUX[2:0]          |   DRDYM   | RESERVED
*/
/* Define I1MUX (current routing) */
#define ADS1220_IDAC1_OFF	0x00
#define ADS1220_IDAC1_AIN0	0x20
#define ADS1220_IDAC1_AIN1	0x40
#define ADS1220_IDAC1_AIN2	0x60
#define ADS1220_IDAC1_AIN3	0x80
#define ADS1220_IDAC1_REFP0	0xa0
#define ADS1220_IDAC1_REFN0	0xc0
/* Define I2MUX (current routing) */
#define ADS1220_IDAC2_OFF	0x00
#define ADS1220_IDAC2_AIN0	0x04
#define ADS1220_IDAC2_AIN1	0x08
#define ADS1220_IDAC2_AIN2	0x0c
#define ADS1220_IDAC2_AIN3	0x10
#define ADS1220_IDAC2_REFP0	0x14
#define ADS1220_IDAC2_REFN0	0x18
/* define DRDYM (DOUT/DRDY behaviour) */
#define ADS1220_DRDY_MODE	0x02

/* type definitions */
typedef int8_t (*ads1220_com_fptr_t)(uint8_t reg_addr,
		uint8_t *data,uint8_t len );

typedef void (*ads1220_delay_fptr_t)(uint32_t period);

/*callback for register access*/
struct ads_dev_t
{
	/*! Read function pointer */
	ads1220_com_fptr_t read;
	/*! Write function pointer */
	ads1220_com_fptr_t write;
	/*!  Delay function pointer */
	ads1220_delay_fptr_t delay_ms;
};

/* Low Level ADS1220 Device Functions */
//void ADS1220Init(void);							/* Device initialization */
//int ADS1220WaitForDataReady(int Timeout);		/* DRDY polling */
//void ADS1220AssertCS(int fAssert);				/* Assert/deassert CS */
//void ADS1220SendByte(unsigned char cData );		/* Send byte to the ADS1220 */
//unsigned char ADS1220ReceiveByte(void);			/* Receive byte from the ADS1220 */
/* ADS1220 Higher Level Functions */
int8_t ADS1220ReadData(int32_t * pData, const struct ads_dev_t *dev);	/* Read the data results */
int8_t ADS1220ReadRegister(uint8_t StartAddress, uint8_t NumRegs, uint8_t * pData,const struct ads_dev_t *dev ); /* Read the register(s) */
int8_t ADS1220WriteRegister(uint8_t StartAddress, uint8_t NumRegs, uint8_t * pData,const struct ads_dev_t *dev ); /* Write the register(s) */
//void ADS1220SendResetCommand(void);				/* Send a device Reset Command */
int8_t ADS1220SendStartCommand(const struct ads_dev_t *dev);				/* Send a Start/SYNC command */
//void ADS1220SendShutdownCommand(void);			/* Place the device in powerdown mode */
/* Register Set Value Commands */
//void ADS1220Config(void);
/*
int ADS1220SetChannel(int Mux);
int ADS1220SetGain(int Gain);
int ADS1220SetPGABypass(int Bypass);
int ADS1220SetDataRate(int DataRate);
int ADS1220SetClockMode(int ClockMode);
int ADS1220SetPowerDown(int PowerDown);
int ADS1220SetTemperatureMode(int TempMode);
int ADS1220SetBurnOutSource(int BurnOut);
int ADS1220SetVoltageReference(int VoltageRef);
int ADS1220Set50_60Rejection(int Rejection);
int ADS1220SetLowSidePowerSwitch(int PowerSwitch);
int ADS1220SetCurrentDACOutput(int CurrentOutput);
int ADS1220SetIDACRouting(int IDACRoute);
int ADS1220SetDRDYMode(int DRDYMode);
*/
/* Register Get Value Commands */
int8_t ADS1220GetChannel(const struct ads_dev_t *dev);
int8_t ADS1220GetGain(const struct ads_dev_t *dev);
int8_t ADS1220GetPGABypass(const struct ads_dev_t *dev);
int8_t ADS1220GetDataRate(const struct ads_dev_t *dev);
int8_t ADS1220GetClockMode(const struct ads_dev_t *dev);
int8_t ADS1220GetPowerDown(const struct ads_dev_t *dev);
int8_t ADS1220GetTemperatureMode(const struct ads_dev_t *dev);
int8_t ADS1220GetBurnOutSource(const struct ads_dev_t *dev);
int8_t ADS1220GetVoltageReference(const struct ads_dev_t *dev);
int8_t ADS1220Get50_60Rejection(const struct ads_dev_t *dev);
int8_t ADS1220GetLowSidePowerSwitch(const struct ads_dev_t *dev);
int8_t ADS1220GetCurrentDACOutput(const struct ads_dev_t *dev);
int8_t ADS1220GetIDACRouting(uint8_t WhichOne,const struct ads_dev_t *dev);
int8_t ADS1220GetDRDYMode(const struct ads_dev_t *dev);
/* Useful Functions within Main Program for Setting Register Contents
*
*  	These functions show the programming flow based on the header definitions.
*  	The calls are not made within the demo example, but could easily be used by calling the function
*  		defined within the program to complete a fully useful program.
*	Similar function calls were made in the firmware design for the ADS1220EVM.
*  
*  The following function calls use ASCII data sent from a COM port to control settings 
*	on the ADS1220.  The data is reconstructed from ASCII and then combined with the
*	register contents to save as new configuration settings.
*
* 	Function names correspond to datasheet register definitions
*/
/*
void set_MUX(char c);
void set_GAIN(char c);
void set_PGA_BYPASS(char c);
void set_DR(char c);
void set_MODE(char c);
void set_CM(char c);
void set_TS(char c);
void set_BCS(char c);
void set_VREF(char c);
void set_50_60(char c);
void set_PSW(char c);
void set_IDAC(char c);
void set_IMUX(char c, int i);
void set_DRDYM(char c);
void set_ERROR(void);
*/

/** Error code definitions */
#define ADS1220_OK                        INT8_C(0)
#define ADS1220_E_NULL_PTR                INT8_C(-1)
#define ADS1220_E_COM_FAIL                INT8_C(-2)
#define ADS1220_E_DEV_NOT_FOUND           INT8_C(-3)
#endif /*ADS1220_H_*/
