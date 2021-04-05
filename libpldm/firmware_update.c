#include "firmware_update.h"
#include <endian.h>

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

	struct pldm_header_info header = {0};
	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_FWUP;
	header.command = PLDM_QUERY_DEVICE_IDENTIFIERS;
	int rc = pack_pldm_header(&header, &(msg->hdr));
	if (PLDM_SUCCESS != rc) {
		return rc;
	}

	return ENCODE_SUCCESS;
}

int decode_query_device_identifiers_resp(const struct pldm_msg *msg,
					 const size_t payload_length,
					 uint8_t *completion_code,
					 uint32_t *device_identifiers_len,
					 uint8_t *descriptor_count,
					 uint8_t **descriptor_data)
{
	if (msg == NULL || completion_code == NULL ||
	    device_identifiers_len == NULL || descriptor_count == NULL ||
	    descriptor_data == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg->payload[0];
	if (PLDM_SUCCESS != *completion_code) {
		return DECODE_SUCCESS;
	}

	if (payload_length < sizeof(struct query_device_identifiers_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct query_device_identifiers_resp *response =
	    (struct query_device_identifiers_resp *)msg->payload;
	*device_identifiers_len = le32toh(response->device_identifiers_len);

	if (*device_identifiers_len < PLDM_FWU_MIN_DESCRIPTOR_IDENTIFIERS_LEN) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	size_t resp_len = sizeof(struct query_device_identifiers_resp);

	if (payload_length != resp_len + *device_identifiers_len) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	*descriptor_count = response->descriptor_count;

	if (*descriptor_count == 0) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*descriptor_data = (uint8_t *)(msg->payload + resp_len);
	return DECODE_SUCCESS;
}
