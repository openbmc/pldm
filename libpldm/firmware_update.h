#ifndef FW_UPDATE_H
#define FW_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif
#include "base.h"
#include "utils.h"

/* inventory commands */
#define PLDM_QUERY_DEVICE_IDENTIFIERS 0x01
#define PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES 0

/** @brief Return code for encode-decode API
 */
enum encode_decode_rc {
	ENCODE_SUCCESS = 0xF0,
	ENCODE_FAILURE = 0xF1,
	DECODE_SUCCESS = 0xF2,
	DECODE_FAILURE = 0xF3
};

/** @brief Create a PLDM request message for QueryDeviceIdentifiers
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @param[in] payload_length - Length of the request message payload
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_query_device_identifiers_req(const uint8_t instance_id,
					struct pldm_msg *msg,
					const size_t payload_length);
#ifdef __cplusplus
}
#endif

#endif // End of FW_UPDATE_H
