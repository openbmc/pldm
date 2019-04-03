#include <endian.h>
#include <string.h>

#include "platform.h"

int encode_set_state_effecter_states_resp(uint8_t instance_id,
					  uint8_t completion_code,
					  struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	msg->payload[0] = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_SET_STATE_EFFECTER_STATES;

	rc = pack_pldm_header(&header, &(msg->hdr));

	return rc;
}

int encode_set_state_effecter_states_req(uint8_t instance_id,
					 uint16_t effecter_id,
					 uint8_t comp_effecter_count,
					 set_effecter_state_field *field,
					 struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_SET_STATE_EFFECTER_STATES;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (comp_effecter_count < 0x1 || comp_effecter_count > 0x8) {
		return PLDM_ERROR_INVALID_DATA;
	}

	uint8_t *encoded_msg = msg->payload;
	effecter_id = htole16(effecter_id);
	memcpy(encoded_msg, &effecter_id, sizeof(effecter_id));
	encoded_msg += sizeof(effecter_id);
	memcpy(encoded_msg, &comp_effecter_count, sizeof(comp_effecter_count));
	encoded_msg += sizeof(comp_effecter_count);
	memcpy(encoded_msg, field,
	       (sizeof(set_effecter_state_field) * comp_effecter_count));

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_resp(const uint8_t *msg,
					  size_t payload_length,
					  uint8_t *completion_code)
{
	if (msg == NULL || completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length > PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*completion_code = msg[0];

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_req(const uint8_t *msg,
					 size_t payload_length,
					 uint16_t *effecter_id,
					 uint8_t *comp_effecter_count,
					 set_effecter_state_field *field)
{
	if (msg == NULL || effecter_id == NULL || comp_effecter_count == NULL ||
	    field == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length > PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*effecter_id = le16toh(*((uint16_t *)msg));
	*comp_effecter_count = *(msg + sizeof(*effecter_id));
	memcpy(field,
	       (msg + sizeof(*effecter_id) + sizeof(*comp_effecter_count)),
	       (sizeof(set_effecter_state_field) * (*comp_effecter_count)));

	return PLDM_SUCCESS;
}
