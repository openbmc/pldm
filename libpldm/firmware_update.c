#include "firmware_update.h"

int encode_query_device_identifiers_req(const uint8_t instance_id,
					struct pldm_msg *msg,
					const size_t payload_length)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	int rc = encode_pldm_header(instance_id, PLDM_FWUP,
				    PLDM_QUERY_DEVICE_IDENTIFIERS, PLDM_REQUEST,
				    msg);

	return (rc == PLDM_SUCCESS) ? ENCODE_SUCCESS : rc;
}
