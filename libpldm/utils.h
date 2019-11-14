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

/** @brief Covert ver32_t to string
 *  @param[in] version - Pointer to ver32_t
 *  @param[out] buffer - Pointer to the buffer
 *  @param[in] buffer_size - Size of the buffer
 *  @return The number of characters(excluding the null byte)
 */
int ver2str(const ver32_t *version, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif