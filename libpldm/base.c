#include <endian.h>
#include <string.h>

#include "base.h"

int pack_pldm_header(const struct pldm_header_info *hdr, struct pldm_msg_t *msg)
{
	if (msg == NULL || hdr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (hdr->msg_type != RESPONSE && hdr->msg_type != REQUEST &&
	    hdr->msg_type != ASYNC_REQUEST_NOTIFY) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (hdr->instance > PLDM_INSTANCE_MAX) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (hdr->pldm_type > (PLDM_MAX_TYPES - 1)) {
		return PLDM_ERROR_INVALID_PLDM_TYPE;
	}

	uint8_t datagram = (hdr->msg_type == ASYNC_REQUEST_NOTIFY) ? 1 : 0;

	if (hdr->msg_type == RESPONSE) {
		msg->request = RESPONSE;
	} else if (hdr->msg_type == REQUEST ||
		   hdr->msg_type == ASYNC_REQUEST_NOTIFY) {
		msg->request = REQUEST;
	}
	msg->datagram = datagram;
	msg->reserved = 0;
	msg->instance_id = hdr->instance;
	msg->header_ver = 0;
	msg->type = hdr->pldm_type;
	msg->command = hdr->command;

	return PLDM_SUCCESS;
}

int unpack_pldm_header(const struct pldm_msg_t *msg, const size_t size,
		       struct pldm_header_info *hdr, size_t *payload_size)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (msg->request == RESPONSE) {
		hdr->msg_type = RESPONSE;
	} else {
		hdr->msg_type =
		    msg->datagram ? ASYNC_REQUEST_NOTIFY : REQUEST;
	}

	hdr->instance = msg->instance_id;
	hdr->pldm_type = msg->type;
	hdr->command = msg->command;
	*payload_size = size - sizeof(struct pldm_msg_t) + sizeof(uint8_t*);

	return PLDM_SUCCESS;
}

int encode_get_types_req(uint8_t instance_id, struct pldm_msg_t *msg)
{
	return PLDM_SUCCESS;
}

int encode_get_commands_req(uint8_t instance_id, uint8_t type,
			    struct pldm_version_t version,
			    struct pldm_msg_t *msg)
{
	uint8_t *dst = msg->payload;
	memcpy(dst, &type, sizeof(type));
	dst += sizeof(type);
	memcpy(dst, &version, sizeof(version));

	return PLDM_SUCCESS;
}

int encode_get_types_resp(uint8_t instance_id, const uint8_t *types,
			  struct pldm_msg_t *msg)
{
	if (msg->payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->payload + sizeof(msg->payload[0]);
		memcpy(dst, types, PLDM_MAX_TYPES / 8);
	}

	return PLDM_SUCCESS;
}

int decode_get_commands_req(const struct pldm_msg_t *msg, uint8_t *type,
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
	if (msg->payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->payload + sizeof(msg->payload[0]);
		memcpy(dst, commands, PLDM_MAX_CMDS_PER_TYPE / 8);
	}

	return PLDM_SUCCESS;
}
