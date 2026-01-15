/*
 * cmd_uicr.c
 *
 *  Created on: May 16, 2019
 *      Author: Ela Asgari
 */

#include <string.h>
#include "cmd_uicr.h"
#include "cmd_protocol.h"
#include "ble_rpc.h"
#include "tskctrl.h"
#include "logger.h"
#include "mx25r6435.h"
//#include "nrf52.h"
#include "nrf52833.h"


static uint32_t *get_address(uicr_src_t src)
{
    if(UICR_SRC_KEY == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[0]);
    if(UICR_SRC_SERIAL_HIGH == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[1]);
    if(UICR_SRC_SERIAL_LOW == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[2]);
    if(UICR_SRC_ENCRYPT_KEY3 == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[3]);
    if(UICR_SRC_ENCRYPT_KEY2 == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[4]);
    if(UICR_SRC_ENCRYPT_KEY1 == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[5]);
    if(UICR_SRC_ENCRYPT_KEY0 == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[6]);
    if(UICR_SRC_SECRET_NUM3 == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[7]);
    if(UICR_SRC_SECRET_NUM2 == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[8]);
    if(UICR_SRC_SECRET_NUM1 == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[9]);
    if(UICR_SRC_SECRET_NUM0 == src)
        return (uint32_t*)(&NRF_UICR->CUSTOMER[10]);
    if(UICR_SRC_APP_PROTECT == src)
        return (uint32_t*)(&NRF_UICR->APPROTECT);
    if(UICR_SRC_NFC_PINS == src)
        return (uint32_t*)(&NRF_UICR->NFCPINS);
    return NULL;
}

uicr_status uicr_read(uicr_src_t src, uint32_t *data)
{
    uint32_t *address = get_address(src);
    if(NULL == address)
        return UICRST_INV_ADD;
    if(NULL == data)
        return UICRST_INV_ADD;
    *data = *address;
    return UICRST_OK;
}

uicr_status uicr_write(uicr_src_t src, uint32_t value)
{
    uint32_t *address = get_address(src);
    if(NULL == address)
        return UICRST_INV_ADD;

    if(BLEST_OP_OK == hal_ble_disconnect())
        hal_ble_deinit_sd();
    log_debug("[uicr] writing to register 0x%08X", address);
    //turn on flash write enable and wait until the NVMC is ready
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    //write memory
    *address = value;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
    //turn off flash write enable and wait until the NVMC is ready
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);

    return UICRST_OK;
}

uicr_status uicr_is_erased(uicr_src_t src, bool *erased)
{
    uint32_t content;
    uicr_status st;
    st = uicr_read(src, &content);
    if(UICRST_OK == st)
        *erased = content == 0xFFFFFFFF;
    return st;
}

uicr_status uicr_disable_debugger(void)
{
    if(0xFFFFFF00 == NRF_UICR->APPROTECT)
        return UICRST_OK;
    return uicr_write(UICR_SRC_APP_PROTECT, 0xFFFFFF00);
}

uicr_status uicr_set_nfc_gpio(void)
{
    if(0xFFFFFFFE == NRF_UICR->NFCPINS)
        return UICRST_OK;
    return uicr_write(UICR_SRC_NFC_PINS, 0xFFFFFFFE);
}

uicr_status uicr_read_serial_number(uint8_t *serial, uint8_t len)
{
    uicr_status st;
    uint32_t temp32;

    if(len != 6)
        return UICRST_INV_LENGTH;

    st = uicr_read(UICR_SRC_SERIAL_LOW, &temp32);
    if(UICRST_OK == st)
    {
        memcpy(serial, &temp32, sizeof(temp32));
        st = uicr_read(UICR_SRC_SERIAL_HIGH, &temp32);
        serial[4] = (uint8_t)(temp32 & 0x000000FF);
        serial[5] = (uint8_t)((temp32 & 0x0000FF00)>>8);
    }
    return st;
}

uicr_status uicr_write_serial_number(uint8_t *serial, uint8_t len)
{
    uint8_t i;
    uicr_status st;
    uint32_t high, low;
    bool erased = false;
    uicr_src_t addr[] = {UICR_SRC_SERIAL_HIGH, UICR_SRC_SERIAL_LOW};

    if((NULL == serial) || (len != 6))
        return UICRST_INV_DATA;

    for(i = 0; i < 2; i++)
    {
        //before writing, check is memory is erased
        st = uicr_is_erased(addr[i], &erased);
        if(UICRST_OK != st)
            return UICRST_FAIL;
        else if(!erased)
            return UICRST_NOT_ERASED;
    }
    memcpy(&low, serial, 4);
    memcpy(&high, &serial[4], 2);
    high |= 0xFFFF0000;
    st = uicr_write(UICR_SRC_SERIAL_LOW, low);
    if(UICRST_OK == st)
    {
        st = uicr_write(UICR_SRC_SERIAL_HIGH, high);
    }
    return st;
}

uicr_status uicr_read_encryption_key(uint8_t *key, uint8_t len)
{
    uicr_status st;
    uint32_t temp32;

    if(len != 16)
        return UICRST_INV_LENGTH;

    for(uint8_t i = 0; i<len/sizeof(temp32); i++)
    {
    	st = uicr_read(UICR_SRC_ENCRYPT_KEY0-i, &temp32);
    	if(UICRST_OK == st)
			memcpy(&key[i*4], &temp32, sizeof(temp32));
    	else
    		break;
    }
    return st;
}

uicr_status uicr_write_encryption_key(uint8_t *key, uint8_t len)
{
    uint8_t i;
    uicr_status st;
    uint32_t temp32;
    bool erased = false;
    uicr_src_t addr[] = {UICR_SRC_ENCRYPT_KEY3, UICR_SRC_ENCRYPT_KEY2,
    		UICR_SRC_ENCRYPT_KEY1, UICR_SRC_ENCRYPT_KEY0};

    if((NULL == key) || (len != 16))
        return UICRST_INV_DATA;

    for(i = 0; i < len/sizeof(temp32); i++)
    {
        //before writing, check is memory is erased
        st = uicr_is_erased(addr[i], &erased);
        if(UICRST_OK != st)
            return UICRST_FAIL;
        else if(!erased)
            return UICRST_NOT_ERASED;
    }
    for(i = 0; i < len/sizeof(temp32); i++)
    {
		memcpy(&temp32, &key[i*4], sizeof(temp32));
		st = uicr_write(UICR_SRC_ENCRYPT_KEY0-i, temp32);
		if(UICRST_OK != st)
		{
			log_debug("UICR failed to write");
			break;
		}
    }
    return st;
}

uicr_status uicr_read_secret_number(uint8_t *secret, uint8_t len)
{
    uicr_status st;
    uint32_t temp32;

    if(len != 16)
        return UICRST_INV_LENGTH;

    for(uint8_t i = 0; i<len/sizeof(temp32); i++)
    {
    	st = uicr_read(UICR_SRC_SECRET_NUM0-i, &temp32);
    	if(UICRST_OK == st)
			memcpy(&secret[i*4], &temp32, sizeof(temp32));
    	else
    		break;
    }
    return st;
}

uicr_status uicr_write_secret_number(uint8_t *secret, uint8_t len)
{
    uint8_t i;
    uicr_status st;
    uint32_t temp32;
    bool erased = false;
    uicr_src_t addr[] = {UICR_SRC_SECRET_NUM3, UICR_SRC_SECRET_NUM2,
    		UICR_SRC_SECRET_NUM1, UICR_SRC_SECRET_NUM0};

    if((NULL == secret) || (len != 16))
        return UICRST_INV_DATA;

    for(i = 0; i < len/sizeof(temp32); i++)
    {
        //before writing, check is memory is erased
        st = uicr_is_erased(addr[i], &erased);
        if(UICRST_OK != st)
            return UICRST_FAIL;
        else if(!erased)
            return UICRST_NOT_ERASED;
    }
    for(i = 0; i < len/sizeof(temp32); i++)
    {
		memcpy(&temp32, &secret[i*4], sizeof(temp32));
		st = uicr_write(UICR_SRC_SECRET_NUM0-i, temp32);
		if(UICRST_OK != st)
		{
			log_debug("UICR failed to write");
			break;
		}
    }
    return st;
}
