#include <endian.h>
#include <string.h>

#include "power_host.h"

int encode_get_alert_status_req(uint8_t instance_id, uint8_t version_id,
				struct pldm_msg *msg, size_t payload_length)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_OEM;
	header.command = PLDM_GET_ALERT_STATUS;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (payload_length != PLDM_GET_ALERT_STATUS_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_alert_status_req *request =
	    (struct pldm_get_alert_status_req *)msg->payload;
	request->version = version_id;

	return PLDM_SUCCESS;
}

int decode_get_alert_status_resp(const struct pldm_msg *msg,
				 size_t payload_length,
				 uint8_t *completion_code, uint32_t *rack_entry,
				 uint32_t *pri_cec_node)
{
	if (msg == NULL || completion_code == NULL || rack_entry == NULL ||
	    pri_cec_node == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg->payload[0];
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	}

	if (payload_length != PLDM_GET_ALERT_STATUS_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_alert_status_resp *response =
	    (struct pldm_get_alert_status_resp *)msg->payload;

	*rack_entry = le32toh(response->rack_entry);
	*pri_cec_node = le32toh(response->pri_cec_node);

	return PLDM_SUCCESS;
}