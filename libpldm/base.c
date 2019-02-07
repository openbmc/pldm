#include <endian.h>
#include <string.h>

#include "base.h"

int pack_pldm_header(uint8_t *ptr, struct pldm_header_info *hdr)
{
	if (ptr == NULL || hdr == NULL) {
		return FAIL;
	}

	if (hdr->msg_type != RESPONSE && hdr->msg_type != REQUEST &&
	    hdr->msg_type != ASYNC_REQUEST_NOTIFY) {
		return FAIL;
	}

	if (hdr->instance > PLDM_INSTANCE_MAX ||
	    hdr->pldm_type > PLDM_TYPE_MAX) {
		return FAIL;
	}

	struct pldm_header_req *header_req = NULL;
	struct pldm_header_resp *header_resp = NULL;
	uint8_t datagram = (hdr->msg_type == ASYNC_REQUEST_NOTIFY) ? 1 : 0;

	if (hdr->msg_type == RESPONSE) {
		header_resp = (struct pldm_header_resp *)ptr;
		header_resp->request = RESPONSE;
		header_resp->datagram = 0;
		header_resp->reserved = 0;
		header_resp->instance_id = hdr->instance;
		header_resp->header_ver = 0;
		header_resp->type = hdr->pldm_type;
		header_resp->command = hdr->command;
		header_resp->completion_code = hdr->completion_code;
	} else if (hdr->msg_type == REQUEST ||
		   hdr->msg_type == ASYNC_REQUEST_NOTIFY) {
		header_req = (struct pldm_header_req *)ptr;
		header_req->request = REQUEST;
		header_req->datagram = datagram;
		header_req->reserved = 0;
		header_req->instance_id = hdr->instance;
		header_req->header_ver = 0;
		header_req->type = hdr->pldm_type;
		header_req->command = hdr->command;
	}

	return OK;
}

int unpack_pldm_header(uint8_t *ptr, size_t size, struct pldm_header_info *hdr,
		       size_t *offset, size_t *payload_size)
{
	if (ptr == NULL) {
		return FAIL;
	}

	struct pldm_header_req *header_req = (struct pldm_header_req *)ptr;
	struct pldm_header_resp *header_resp = NULL;

	if (header_req->request == RESPONSE) {
		if (size < PLDM_RESPONSE_HEADER_SIZE) {
			return FAIL;
		}
		header_resp = (struct pldm_header_resp *)ptr;
		hdr->msg_type = RESPONSE;
		hdr->completion_code = header_resp->completion_code;
		*offset = PLDM_RESPONSE_HEADER_SIZE;
		*payload_size = size - PLDM_RESPONSE_HEADER_SIZE;
	} else {
		if (size < PLDM_REQUEST_HEADER_SIZE) {
			return FAIL;
		}
		hdr->msg_type =
		    header_req->datagram ? ASYNC_REQUEST_NOTIFY : REQUEST;
		*offset = PLDM_REQUEST_HEADER_SIZE;
		*payload_size = size - PLDM_REQUEST_HEADER_SIZE;
	}

	hdr->instance = header_req->instance_id;
	hdr->pldm_type = header_req->type;
	hdr->command = header_req->command;

	return OK;
}

int encode_get_types_req(uint8_t instance_id, uint8_t *buffer) { return OK; }

int encode_get_commands_req(uint8_t instance_id, uint8_t type, ver32 version,
			    uint8_t *buffer)
{
	uint8_t *dst = buffer + PLDM_REQUEST_HEADER_LEN_BYTES;
	memcpy(dst, &type, sizeof(type));
	dst += sizeof(type);
	version = htole32(version);
	memcpy(dst, &version, sizeof(version));

        return OK;
}

int encode_get_types_resp(uint8_t instance_id, uint8_t completion_code,
			  const uint8_t *types, uint8_t *buffer)
{
	if (completion_code == SUCCESS) {
		uint8_t *dst = buffer + PLDM_RESPONSE_HEADER_LEN_BYTES;
		memcpy(dst, types, PLDM_MAX_TYPES / 8);
	}

	return OK;
}

int decode_get_commands_req(const uint8_t *request, uint8_t *type,
			    ver32 *version)
{
	*type = *(request + PLDM_REQUEST_HEADER_LEN_BYTES);
	*version = *((ver32 *)(type + sizeof(uint8_t)));
	*version = le32toh(*version);

	return OK;
}

int encode_get_commands_resp(uint8_t instance_id, uint8_t completion_code,
			     const uint8_t *commands, uint8_t *buffer)
{
	if (completion_code == SUCCESS) {
		uint8_t *dst = buffer + PLDM_RESPONSE_HEADER_LEN_BYTES;
		memcpy(dst, commands, PLDM_MAX_CMDS_PER_TYPE / 8);
	}

	return OK;
}
