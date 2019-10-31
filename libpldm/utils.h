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

/** @brief Judge whether the input time is legal
 *
 *  @param[in] seconds - Seconds in BCD format
 *  @param[in] minutes - minutes in BCD format
 *  @param[in] hours - hours in BCD format
 *  @param[in] day - day of month in BCD format
 *  @param[in] month - month in BCD format
 *  @param[in] year - year in BCD format
 *  @return 0 or 1
 */
int judge_time_legal(uint8_t seconds, uint8_t minutes, uint8_t hours,
		     uint8_t day, uint8_t month, uint16_t year);

#ifdef __cplusplus
}
#endif

#endif