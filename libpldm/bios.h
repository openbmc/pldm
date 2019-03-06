#ifndef BIOS_H
#define BIOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <stddef.h>
#include <stdint.h>

#include "base.h"
#include "bios.h"

/* Response lengths are inclusive of completion code */
#define PLDM_GET_DATE_TIME_RESP_BYTES 8

/* Requester */

/* GetDateTime */

/** @brief Create a PLDM request message for GetDateTime
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */

int encode_get_dateTime_req(uint8_t instance_id, struct pldm_msg *msg);

/** @brief Decode a GetDateTime response message
 *
 *  @param[in] msg - Response message payload
 *  param[out] seconds - seconds elapsed
 *  param[out] minutes - minutes elapsed
 *  param[out] hours - hours elapsed
 *  param[out] day - day of month
 *  param[out] month - number of month
 *  param[out] year - year number
 *  @return pldm_completion_codes
 */
int decode_get_dateTime_resp(const struct pldm_msg_payload *msg,
			     uint8_t *seconds, uint8_t *minutes, uint8_t *hours,
			     uint8_t *day, uint8_t *month, uint16_t *year);

/* Responder */

/* GetDateTime */

/** @brief Create a PLDM response message for GetDateTime
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] seconds - number of seconds elapsed
 *  @param[in] minutes - number of minutes elapsed
 *  @param[in] hours - number of hours elapsed
 *  @param[in] day - day of the month
 *  @param[in] month - month number
 *  @param[in] year - year number
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */

int encode_get_dateTime_resp(uint8_t instance_id, uint8_t completion_code,
			     const uint8_t *seconds, const uint8_t *minutes,
			     const uint8_t *hours, const uint8_t *day,
			     const uint8_t *month, const uint16_t *year,
			     struct pldm_msg *msg);

#ifdef __cplusplus
}
#endif

#endif /* BIOS_H */
