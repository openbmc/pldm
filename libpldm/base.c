#include <endian.h>
#include <string.h>

#include "base.h"

int encode_get_types_req(uint8_t instance_id, uint8_t *buffer)
{
	return PLDM_SUCCESS;
}

int pack_pldm_header(const struct pldm_header_info *hdr, uint8_t *ptr)
{
	if (ptr == NULL || hdr == NULL) {
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

	return PLDM_SUCCESS;
}

int unpack_pldm_header(const uint8_t *ptr, const size_t size,
		       struct pldm_header_info *hdr, size_t *offset,
		       size_t *payload_size)
{
	if (ptr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct pldm_header_req *header_req = (struct pldm_header_req *)ptr;
	struct pldm_header_resp *header_resp = NULL;

	if (header_req->request == RESPONSE) {
		if (size < PLDM_RESPONSE_HEADER_LEN_BYTES) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		header_resp = (struct pldm_header_resp *)ptr;
		hdr->msg_type = RESPONSE;
		hdr->completion_code = header_resp->completion_code;
		*offset = PLDM_RESPONSE_HEADER_LEN_BYTES;
		*payload_size = size - PLDM_RESPONSE_HEADER_LEN_BYTES;
	} else {
		if (size < PLDM_REQUEST_HEADER_LEN_BYTES) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		hdr->msg_type =
		    header_req->datagram ? ASYNC_REQUEST_NOTIFY : REQUEST;
		*offset = PLDM_REQUEST_HEADER_LEN_BYTES;
		*payload_size = size - PLDM_REQUEST_HEADER_LEN_BYTES;
	}

	hdr->instance = header_req->instance_id;
	hdr->pldm_type = header_req->type;
	hdr->command = header_req->command;

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
	*type = *(request + PLDM_REQUEST_HEADER_LEN_BYTES);
	memcpy(version, (struct pldm_version_t *)(type + sizeof(*type)),
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
