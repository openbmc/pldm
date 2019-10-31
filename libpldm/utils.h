#ifndef UTILS_H__
#define UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/** @brief Compute Crc32(same as the one used by IEEE802.3)
 *
 *  @param[in] data - Pointer to the target data
 *  @param[in] size - Size of the data
 *  @return The checksum
 */
uint32_t crc32(const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif