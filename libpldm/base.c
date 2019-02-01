#include <endian.h>
#include <string.h>

#include "base.h"

int encode_get_types_req(uint8_t instance_id, uint8_t *buffer)
{
	return PLDM_SUCCESS;
}

int encode_get_commands_req(uint8_t instance_id, uint8_t type,
			    struct pldm_version_t version, uint8_t *buffer)
{
	uint8_t *dst = buffer + PLDM_REQUEST_HEADER_LEN_BYTES;
	memcpy(dst, &type, sizeof(type));
	dst += sizeof(type);
	memcpy(dst, &version, sizeof(version));

	return PLDM_SUCCESS;
}

int encode_get_types_resp(uint8_t instance_id, uint8_t completion_code,
			  const uint8_t *types, uint8_t *buffer)
{
	if (completion_code == PLDM_SUCCESS) {
		uint8_t *dst = buffer + PLDM_RESPONSE_HEADER_LEN_BYTES;
		memcpy(dst, types, PLDM_MAX_TYPES / 8);
	}

	return PLDM_SUCCESS;
}

int decode_get_commands_req(const uint8_t *request, uint8_t *type,
			    struct pldm_version_t *version)
{
	const uint8_t *start = request + PLDM_REQUEST_HEADER_LEN_BYTES;
	*type = *start;
	memcpy(version, (struct pldm_version_t *)(start + sizeof(*type)),
	       sizeof(*version));

	return PLDM_SUCCESS;
}

int encode_get_commands_resp(uint8_t instance_id, uint8_t completion_code,
			     const uint8_t *commands, uint8_t *buffer)
{
	if (completion_code == PLDM_SUCCESS) {
		uint8_t *dst = buffer + PLDM_RESPONSE_HEADER_LEN_BYTES;
		memcpy(dst, commands, PLDM_MAX_CMDS_PER_TYPE / 8);
	}

	return PLDM_SUCCESS;
}
