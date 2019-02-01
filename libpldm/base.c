#include <endian.h>
#include <string.h>

#include "base.h"

int encode_get_types_req(uint8_t instance_id, struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	return PLDM_SUCCESS;
}

int encode_get_commands_req(uint8_t instance_id, uint8_t type,
			    struct pldm_version version, struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	uint8_t *dst = msg->body.payload;
	memcpy(dst, &type, sizeof(type));
	dst += sizeof(type);
	memcpy(dst, &version, sizeof(version));

	return PLDM_SUCCESS;
}

int encode_get_types_resp(uint8_t instance_id, uint8_t completion_code,
			  const uint8_t *types, struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	msg->body.payload[0] = completion_code;
	if (msg->body.payload[0] == PLDM_SUCCESS) {
		if (types == NULL) {
			return PLDM_ERROR_INVALID_DATA;
		}
		uint8_t *dst = msg->body.payload + sizeof(msg->body.payload[0]);
		memcpy(dst, types, PLDM_MAX_TYPES / 8);
	}

	return PLDM_SUCCESS;
}

int decode_get_commands_req(const struct pldm_msg_payload *msg, uint8_t *type,
			    struct pldm_version *version)
{
	if (msg == NULL || type == NULL || version == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	const uint8_t *start = msg->payload;
	*type = *start;
	memcpy(version, (struct pldm_version *)(start + sizeof(*type)),
	       sizeof(*version));

	return PLDM_SUCCESS;
}

int encode_get_commands_resp(uint8_t instance_id, uint8_t completion_code,
			     const uint8_t *commands, struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	msg->body.payload[0] = completion_code;
	if (msg->body.payload[0] == PLDM_SUCCESS) {
		if (commands == NULL) {
			return PLDM_ERROR_INVALID_DATA;
		}
		uint8_t *dst = msg->body.payload + sizeof(msg->body.payload[0]);
		memcpy(dst, commands, PLDM_MAX_CMDS_PER_TYPE / 8);
	}

	return PLDM_SUCCESS;
}

int decode_get_types_resp(const struct pldm_msg_payload *msg, uint8_t *types)
{
	if (msg == NULL || types == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	const uint8_t *src = msg->payload + sizeof(uint8_t);
	memcpy(types, src, PLDM_MAX_TYPES / 8);

	return PLDM_SUCCESS;
}

int decode_get_commands_resp(const struct pldm_msg_payload *msg,
			     uint8_t *commands)
{
	if (msg == NULL || commands == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	const uint8_t *src = msg->payload + sizeof(uint8_t);
	memcpy(commands, src, PLDM_MAX_CMDS_PER_TYPE / 8);

	return PLDM_SUCCESS;
}
