#include <string.h>
#include <endian.h>

#include "base.h"

int encode_get_types_req(uint8_t instance_id,
			 uint8_t *buffer)
{
}

int encode_get_commands_req(uint8_t instance_id,
			    uint8_t type,
			    ver32 version,
			    uint8_t *buffer)
{
	uint8_t *dst = buffer + REQUEST_HEADER_LEN_BYTES;
	memcpy(dst, &type, sizeof(type));
	dst += sizeof(type);
	version = htole32(version);
	memcpy(dst, &version, sizeof(version));

        return OK;
}

int encode_get_types_resp(uint8_t instance_id,
			  uint8_t completion_code,
			  const uint8_t *types,
			  uint8_t *buffer)
{
	if (completion_code == SUCCESS) {
		uint8_t *dst = buffer + RESPONSE_HEADER_LEN_BYTES;
		memcpy(dst, types, MAX_TYPES/sizeof(uint8_t));
	}

        return OK;
}

int decode_get_commands_req(const uint8_t *request,
			    uint8_t *type,
			    ver32 *version)
{
	*type = *(request + REQUEST_HEADER_LEN_BYTES);
	*version = *((ver32*)(type + sizeof(uint8_t)));
	*version = le32toh(*version);

	return OK;
}

int encode_get_commands_resp(uint8_t instance_id,
			     uint8_t completion_code,
			     const uint8_t *commands,
			     uint8_t *buffer)
{
	if (completion_code == SUCCESS) {
		uint8_t *dst = buffer + RESPONSE_HEADER_LEN_BYTES;
		memcpy(dst, commands, MAX_CMDS_PER_TYPE/sizeof(uint8_t));
	}

        return OK;
}
