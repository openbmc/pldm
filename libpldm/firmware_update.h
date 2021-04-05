#ifndef FW_UPDATE_H
#define FW_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif
#include "base.h"

#define PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES 0

/** @brief PLDM Firmware update commands
 */
enum pldm_firmware_update_commands { PLDM_QUERY_DEVICE_IDENTIFIERS = 0x01 };

/** @brief Create a PLDM request message for QueryDeviceIdentifiers
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] payload_length - Length of the request message payload
 *  @param[in,out] msg - Message will be written to this
 *
 *  @return pldm_completion_codes
 *
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_query_device_identifiers_req(uint8_t instance_id,
					size_t payload_length,
					struct pldm_msg *msg);
#ifdef __cplusplus
}
#endif

#endif // End of FW_UPDATE_H
