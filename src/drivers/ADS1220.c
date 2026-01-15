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
 * ADS1220.c
 *
 */
/******************************************************************************
// ADS1220 Demo C Function Calls  
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
//   
 ******************************************************************************/
#include "ADS1220.h"

/*
******************************************************************************
 higher level functions
*/
int8_t ADS1220ReadData(int32_t * pData, const struct ads_dev_t *dev)
{
    int8_t rslt = ADS1220_OK;
   	uint8_t cmd_byte;
   	uint8_t result[3];
   	if ((dev == NULL) || (dev->read == NULL))
   	{
   		rslt = ADS1220_E_NULL_PTR;
   	}
   	else
   	{
   		/* get the command byte */
   		cmd_byte=ADS1220_CMD_RDATA;
   		rslt=dev->read(cmd_byte, result, 3);
   	}
   	*pData=(result[0]<<16)|(result[1]<<8)|result[2];
     /* sign extend data */
   	if (*pData & 0x800000)
   		*pData = (*pData)|0xff000000;
    return rslt;
}
int8_t ADS1220ReadRegister(uint8_t StartAddress, uint8_t NumRegs, uint8_t * pData,const struct ads_dev_t *dev )
{
	int8_t rslt = ADS1220_OK;
	uint8_t cmd_byte;
	if ((dev == NULL) || (dev->read == NULL))
	{
		rslt = ADS1220_E_NULL_PTR;
	}
	else
	{
		/* get the command byte */
		cmd_byte=ADS1220_CMD_RREG | (((StartAddress<<2) & 0x0c) |((NumRegs-1)&0x03));
		rslt=dev->read(cmd_byte, pData, NumRegs);
	}
   	return rslt;
}
int8_t ADS1220WriteRegister(uint8_t StartAddress, uint8_t NumRegs, uint8_t * pData,const struct ads_dev_t *dev )
{
	int8_t rslt = ADS1220_OK;
	uint8_t cmd_byte;
	if ((dev == NULL) || (dev->read == NULL))
	{
		rslt = ADS1220_E_NULL_PTR;
	}
	else
	{
		/* get the command byte */
		cmd_byte=ADS1220_CMD_WREG | (((StartAddress<<2) & 0x0c) |((NumRegs-1)&0x03));
		rslt=dev->write(cmd_byte, pData, NumRegs);
	}
  	return rslt;
}
int8_t ADS1220SendStartCommand(const struct ads_dev_t *dev)
{
	int8_t rslt = ADS1220_OK;
	uint8_t cmd_byte;
	if ((dev == NULL) || (dev->read == NULL))
	{
		rslt = ADS1220_E_NULL_PTR;
	}
	else
	{
		/* get the command byte */
		cmd_byte=ADS1220_CMD_SYNC;
		rslt=dev->write(cmd_byte, NULL, 0);
	}
	return rslt;
}

/*
******************************************************************************
register get value commands
*/
int8_t ADS1220GetChannel(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse Mux data from register */
	ADS1220ReadRegister(ADS1220_0_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return (Temp >>4);
}
int8_t ADS1220GetGain(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse Gain data from register */
	ADS1220ReadRegister(ADS1220_0_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( (Temp & 0x0e) >>1);
}
int8_t ADS1220GetPGABypass(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse Bypass data from register */
	ADS1220ReadRegister(ADS1220_0_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return (Temp & 0x01);
}
int8_t ADS1220GetDataRate(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse DataRate data from register */
	ADS1220ReadRegister(ADS1220_1_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( Temp >>5 );
}
int8_t ADS1220GetClockMode(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse ClockMode data from register */
	ADS1220ReadRegister(ADS1220_1_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( (Temp & 0x18) >>3 );
}
int8_t ADS1220GetPowerDown(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse PowerDown data from register */
	ADS1220ReadRegister(ADS1220_1_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( (Temp & 0x04) >>2 );
}
int8_t ADS1220GetTemperatureMode(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse TempMode data from register */
	ADS1220ReadRegister(ADS1220_1_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( (Temp & 0x02) >>1 );
}
int8_t ADS1220GetBurnOutSource(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse BurnOut data from register */
	ADS1220ReadRegister(ADS1220_1_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( Temp & 0x01 );
}
int8_t ADS1220GetVoltageReference(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse VoltageRef data from register */
	ADS1220ReadRegister(ADS1220_2_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( Temp >>6 );
}
int8_t ADS1220Get50_60Rejection(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse Rejection data from register */
	ADS1220ReadRegister(ADS1220_2_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( (Temp & 0x30) >>4 );
}
int8_t ADS1220GetLowSidePowerSwitch(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse PowerSwitch data from register */
	ADS1220ReadRegister(ADS1220_2_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( (Temp & 0x08) >>3);
}
int8_t ADS1220GetCurrentDACOutput(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse IDACOutput data from register */
	ADS1220ReadRegister(ADS1220_2_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( Temp & 0x07 );
}
int8_t ADS1220GetIDACRouting(uint8_t WhichOne,const struct ads_dev_t *dev)
{
	/* Check WhichOne sizing */
	if (WhichOne >1) return ADS1220_ERROR;
	uint8_t Temp;
	/* Parse Mux data from register */
	ADS1220ReadRegister(ADS1220_3_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	if (WhichOne) return ( (Temp & 0x1c) >>2);
	else return ( Temp >>5 );
}
int8_t ADS1220GetDRDYMode(const struct ads_dev_t *dev)
{
	uint8_t Temp;
	/* Parse DRDYMode data from register */
	ADS1220ReadRegister(ADS1220_3_REGISTER, 0x01, &Temp,dev);
	/* return the parsed data */
	return ( (Temp & 0x02) >>1 );
}
