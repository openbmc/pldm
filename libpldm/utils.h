#ifndef UTILS_H__
#define UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "pldm_types.h"
#include <stddef.h>
#include <stdint.h>

/** @brief Compute Crc32(same as the one used by IEEE802.3)
 *
 *  @param[in] data - Pointer to the target data
 *  @param[in] size - Size of the data
 *  @return The checksum
 */
uint32_t crc32(const void *data, size_t size);

/** @brief Convert ver32_t to string
 *  @param[in] version - Pointer to ver32_t
 *  @param[out] buffer - Pointer to the buffer
 *  @param[in] buffer_size - Size of the buffer
 *  @return The number of characters(excluding the null byte) or negative if
 * error is encountered
 */
int ver2str(const ver32_t *version, char *buffer, size_t buffer_size);

/** @breif Convert bcd number(uint8_t) to decimal
 *  @param[in] bcd - bcd number
 *  @return the decimal number
 */
uint8_t bcd2dec8(uint8_t bcd);

/** @breif Convert decimal number(uint8_t) to bcd
 *  @param[in] dec - decimal number
 *  @return the bcd number
 */
uint8_t dec2bcd8(uint8_t dec);

/** @breif Convert bcd number(uint16_t) to decimal
 *  @param[in] bcd - bcd number
 *  @return the decimal number
 */
uint16_t bcd2dec16(uint16_t bcd);

/** @breif Convert decimal number(uint16_t) to bcd
 *  @param[in] dec - decimal number
 *  @return the bcd number
 */
uint16_t dec2bcd16(uint16_t dec);

/** @breif Convert bcd number(uint32_t) to decimal
 *  @param[in] bcd - bcd number
 *  @return the decimal number
 */
uint32_t bcd2dec32(uint32_t bcd);

/** @breif Convert decimal number(uint32_t) to bcd
 *  @param[in] dec - decimal number
 *  @return the bcd number
 */
uint32_t dec2bcd32(uint32_t dec);

#ifdef __cplusplus
}
#endif

#endif
