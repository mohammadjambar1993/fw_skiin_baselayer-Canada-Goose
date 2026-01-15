/*
 * mya_str.h
 *
 *  Created on: Sep 21, 2017
 *      Author: Myant
 */

#ifndef SRC_UTIL_MYA_STR_H_
#define SRC_UTIL_MYA_STR_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

bool str_2int(char *str, int64_t *num, int base);
bool str_2uint(char *str, uint64_t *num, int base);
bool str_equal(char *s1, char *s2, uint16_t maxlength);
bool int_2str(uint32_t num, char *buffer, uint32_t size, int base);
uint16_t str_length(const char *str, uint16_t max_length);
uint16_t str_copy(char *origin, char *destiny, uint16_t maxlength);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* SRC_UTIL_MYA_STR_H_ */
