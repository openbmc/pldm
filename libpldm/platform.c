#include <endian.h>
#include <string.h>

#include "platform.h"

int encode_set_state_effecter_states_resp(uint8_t instance_id,
					  uint8_t completion_code,
					  struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	msg->body.payload[0] = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_SET_STATE_EFFECTER_STATES;

	rc = pack_pldm_header(&header, &(msg->hdr));

	return rc;
}

int encode_set_state_effecter_states_req(
    uint8_t instance_id, uint16_t effecter_id, uint8_t comp_effecter_count,
    set_effecter_state_field *stateField, uint8_t stateField_size, struct pldm_msg *msg)
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

	uint8_t *encoded_msg = msg->body.payload;
	effecter_id = htole16(effecter_id);
	memcpy(encoded_msg, &effecter_id, sizeof(effecter_id));
	encoded_msg += sizeof(effecter_id);
	memcpy(encoded_msg, &comp_effecter_count, sizeof(comp_effecter_count));
	encoded_msg += sizeof(comp_effecter_count);
	memcpy(encoded_msg, stateField, stateField_size);

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_resp(const struct pldm_msg_payload *msg,
					  uint8_t *completion_code)
{
	if (msg == NULL || completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = *(uint8_t *)msg->payload;

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_req(
    const struct pldm_msg_payload *msg, uint16_t *effecter_id,
    uint8_t *comp_effecter_count, set_effecter_state_field *field)
{
	if (msg == NULL || effecter_id == NULL || comp_effecter_count == NULL ||
	    field == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	const uint8_t *start = msg->payload;
	*effecter_id = le16toh(*((uint16_t *)start));
	*comp_effecter_count = *(start + sizeof(*effecter_id));
	memcpy(field,
	       (start + sizeof(*effecter_id) + sizeof(*comp_effecter_count)),
	       (sizeof(set_effecter_state_field) * (*comp_effecter_count)));

	return PLDM_SUCCESS;
}
