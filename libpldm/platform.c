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
	header.command = PLDM_SET_STATE_EFFECTER_STATE;

	rc = pack_pldm_header(&header, &(msg->hdr));

	return rc;
}

int decode_set_state_effecter_states_req(
    const struct pldm_msg_payload *msg, uint16_t *effecter_id,
    uint8_t *comp_effecter_count, state_field_set_state_effecter_state *field)
{
	if (msg == NULL || effecter_id == NULL || comp_effecter_count == NULL ||
	    field == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	const uint8_t *start = msg->payload;
	*effecter_id = le16toh(*((uint16_t *)start));
	*comp_effecter_count = *(start + sizeof(*effecter_id));
	*field = *((state_field_set_state_effecter_state
			*)(start + sizeof(*effecter_id) +
			   sizeof(*comp_effecter_count)));

	return PLDM_SUCCESS;
}
