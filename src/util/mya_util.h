/*
 * cli_common.h
 *
 *  Created on: Aug 26, 2016
 *      Author: Myant
 */

#ifndef SRC_MYA_UTIL_H_
#define SRC_MYA_UTIL_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#define IS_BIG_ENDIAN() (*(uint16_t *)"\0\xff" < 0x100)

#define read_bit(x,pos) ((x&(1<<pos))>>pos)
#define clr_bit(x,pos)  ((x) &= ~((1) << (pos)))
#define set_bit(x,pos)  ((x) |= ((1) << (pos)))

uint16_t util_byteslen(uint64_t num);
void util_change_endianness(void *data, uint16_t len);
bool util_blocking_delay_us(uint32_t count);
bool util_blocking_delay_ms(uint32_t count);
bool util_check_flag_us(volatile bool *flag, uint32_t timeout_us);
bool util_check_flag_ms(bool *flag, uint32_t timeout_ms);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_MYA_UTIL_H_ */
