/*
 * appconfig.h
 *
 *  Created on: Jul 26 16, 2019
 *      Author: Myant
 */
#pragma once
#ifndef SRC_APPCONFIG_H_
#define SRC_APPCONFIG_H_

#include "FreeRTOS.h"

//firmware version
#define FWV_MAJOR       5
#define FWV_MINOR       5
#define FWV_PATCH       1
#define FWV_BUILD       1

//select PCB id
#define PCB_MULTI_CHANNEL	3
#define PCB_DUAL_CHANNEL	4
#define PCB_ID				PCB_MULTI_CHANNEL

//set BLE advertising name (must be 4 chars long)
#if PCB_ID == PCB_MULTI_CHANNEL
#define BLE_NAME	"Mchx"
#elif PCB_ID == PCB_DUAL_CHANNEL
#define BLE_NAME	"DcS1"
#endif

//BLE heat proprietary service UUID
#define SKIIN_MODULE_ID	0x6200

//hardware version
#define HW_VERSION      PCB_ID

#define BLE_TX_POWER            0   //BLE tx power in dBm
#define ENABLE_CHECK_STACK      1	//enable stack check task
#define ENABLE_CLI              1   //enable command line interface
#define ENABLE_LOGGER           1   //enable logging with segger RTT
#define ENABLE_IO_LOGGER        1   //use io pins to signal events
#define BLE_SECURED 			0   //disable security link
#define CONFIG_UICR  			0	//disable debugger access
#define GTK_SPI_ON      		1	//SPI gatekeeper enabled
/* ENABLE_TEST_FUNCTIONS: enable functions that should be only available for
 * debugging and tests. This shouldn't be enabled for production! */
#define ENABLE_TEST_FUNCTIONS   0
#define ENABLE_ADS_TEST		    1

//ble errors
#define ERRLED_BLE                  COLOR_RED
#define ERRBLINK_BLE_DEFCONFIG      1
#define ERRBLINK_BLE_ENABLED        2
#define ERRBLINK_BLE_EVTCALLBACK    3
#define ERRBLINK_BLE_SYSCALLBACK    4
#define ERRBLINK_BLE_GAPPARAMS      5
#define ERRBLINK_BLE_APPEARANCE     6
#define ERRBLINK_BLE_UUIDADD        7
#define ERRBLINK_BLE_SERVICEADD     8
#define ERRBLINK_BLE_CHARADD_UUID   9
#define ERRBLINK_BLE_CHARADD        10

#endif /* SRC_APPCONFIG_H_ */
