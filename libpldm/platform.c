#include <endian.h>
#include <string.h>

#include "platform.h"

int encode_set_state_effecter_states_resp(uint8_t instance_id,
					  uint8_t completion_code,
					  struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;
	struct PLDM_SetStateEffecterStates_Response *dst =
	    (struct PLDM_SetStateEffecterStates_Response *)msg->body.payload;

	dst->completion_code = completion_code;

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

	struct PLDM_SetStateEffecterStates_Request *encoded_msg =
	    (struct PLDM_SetStateEffecterStates_Request *)msg->body.payload;
	effecter_id = htole16(effecter_id);
	encoded_msg->effecter_id = effecter_id;
	encoded_msg->comp_effecter_count = comp_effecter_count;
	memcpy(encoded_msg->field, field,
	       (sizeof(set_effecter_state_field) * comp_effecter_count));

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_resp(const struct pldm_msg_payload *msg,
					  uint8_t *completion_code)
{
	if (msg == NULL || completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	struct PLDM_SetStateEffecterStates_Response *dst =
	    (struct PLDM_SetStateEffecterStates_Response *)msg->payload;

	*completion_code = dst->completion_code;

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_req(const struct pldm_msg_payload *msg,
					 uint16_t *effecter_id,
					 uint8_t *comp_effecter_count,
					 set_effecter_state_field *field)
{
	if (msg == NULL || effecter_id == NULL || comp_effecter_count == NULL ||
	    field == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	struct PLDM_SetStateEffecterStates_Request *start =
	    (struct PLDM_SetStateEffecterStates_Request *)msg->payload;

	*effecter_id = le16toh(start->effecter_id);
	*comp_effecter_count = start->comp_effecter_count;
	memcpy(field, start->field,
	       (sizeof(set_effecter_state_field) * (*comp_effecter_count)));

	return PLDM_SUCCESS;
}
