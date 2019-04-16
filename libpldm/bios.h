#ifndef BIOS_H
#define BIOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <stddef.h>
#include <stdint.h>

#include "base.h"

/* Response lengths are inclusive of completion code */
#define PLDM_GET_DATE_TIME_RESP_BYTES 8

enum pldm_bios_commands { PLDM_GET_DATE_TIME = 0x0c };

/* Requester */

/* GetDateTime */

/** @brief Create a PLDM request message for GetDateTime
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */

int encode_get_date_time_req(uint8_t instance_id, struct pldm_msg *msg);

/** @brief Decode a GetDateTime response message
 *
 *  @param[in] msg - Response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] seconds - Seconds in BCD format
 *  @param[out] minutes - minutes in BCD format
 *  @param[out] hours - hours in BCD format
 *  @param[out] day - day of month in BCD format
 *  @param[out] month - number of month in BCD format
 *  @param[out] year - year in BCD format
 *  @return pldm_completion_codes
 */
int decode_get_date_time_resp(const struct pldm_msg_payload *msg,
			      uint8_t *completion_code, uint8_t *seconds,
			      uint8_t *minutes, uint8_t *hours, uint8_t *day,
			      uint8_t *month, uint16_t *year);

/* Responder */

/* GetDateTime */

/** @brief Create a PLDM response message for GetDateTime
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] seconds - seconds in BCD format
 *  @param[in] minutes - minutes in BCD format
 *  @param[in] hours - hours in BCD format
 *  @param[in] day - day of the month in BCD format
 *  @param[in] month - number of month in BCD format
 *  @param[in] year - year in BCD format
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */

int encode_get_date_time_resp(uint8_t instance_id, uint8_t completion_code,
			      uint8_t seconds, uint8_t minutes, uint8_t hours,
			      uint8_t day, uint8_t month, uint16_t year,
			      struct pldm_msg *msg);

#ifdef __cplusplus
}
#endif

#endif /* BIOS_H */
