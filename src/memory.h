/*
 * memory.h
 *
 *  Created on: Mar 11, 2021
 *      Author: Jeffrey Zhu
 */

#ifndef SRC_MEMORY_H_
#define SRC_MEMORY_H_

#include <stdbool.h>
#include <stdint.h>
#include "mx25r6435.h"

memory_status_t mem_get_flash_id(uint16_t *id);
bool memory_init(void);
memory_status_t mem_read(uint32_t addr, void *data, uint16_t len);
memory_status_t mem_write(uint32_t addr, void *data, uint16_t len);
memory_status_t mem_read_page(uint16_t page, uint32_t addr, void *data, uint16_t len);
memory_status_t mem_write_page(uint16_t page, uint16_t addr, void *data, uint16_t len);
memory_status_t mem_erase_page(uint32_t addr);
memory_status_t mem_read_status(uint8_t *status);
memory_status_t mem_power_up(void);
memory_status_t mem_power_down(void);
bool mem_is_busy(void);
#if CLI_SUPPORT_ON == 1
memory_status_t mem_set_trace(bool enabled);
#endif

#endif /* SRC_MEMORY_H_ */
