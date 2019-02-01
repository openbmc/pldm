#include <endian.h>
#include <string.h>

#include "base.h"

int encode_get_types_req(uint8_t instance_id, struct pldm_msg_t *msg)
{
	return PLDM_SUCCESS;
}

int encode_get_commands_req(uint8_t instance_id, uint8_t type,
			    struct pldm_version_t version,
			    struct pldm_msg_t *msg)
{
	uint8_t *dst = msg->body.payload;
	memcpy(dst, &type, sizeof(type));
	dst += sizeof(type);
	memcpy(dst, &version, sizeof(version));

	return PLDM_SUCCESS;
}

int encode_get_types_resp(uint8_t instance_id, const uint8_t *types,
			  struct pldm_msg_t *msg)
{
	if (msg->body.payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->body.payload + sizeof(msg->body.payload[0]);
		memcpy(dst, types, PLDM_MAX_TYPES / 8);
	}

	return PLDM_SUCCESS;
}

int decode_get_commands_req(const struct pldm_msg_payload_t *msg, uint8_t *type,
			    struct pldm_version_t *version)
{
	const uint8_t *start = msg->payload;
	*type = *start;
	memcpy(version, (struct pldm_version_t *)(start + sizeof(*type)),
	       sizeof(*version));

	return PLDM_SUCCESS;
}

int encode_get_commands_resp(uint8_t instance_id, const uint8_t *commands,
			     struct pldm_msg_t *msg)
{
	if (msg->body.payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->body.payload + sizeof(msg->body.payload[0]);
		memcpy(dst, commands, PLDM_MAX_CMDS_PER_TYPE / 8);
	}

	return PLDM_SUCCESS;
}
