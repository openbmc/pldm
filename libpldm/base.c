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
		hdr->msg_type = msg->datagram ? ASYNC_REQUEST_NOTIFY : REQUEST;
	}

	hdr->instance = msg->instance_id;
	hdr->pldm_type = msg->type;
	hdr->command = msg->command;
	*payload_size = size - sizeof(struct pldm_msg_t) + sizeof(uint8_t *);

	return PLDM_SUCCESS;
}

int encode_get_types_req(uint8_t instance_id, struct pldm_msg_t *msg)
{
	struct pldm_header_info header = {0};
	header.instance = instance_id;
	header.msg_type = REQUEST;
	header.command = PLDM_GET_PLDM_TYPES;
	pack_pldm_header(&header, msg);

	return PLDM_SUCCESS;
}

int encode_get_commands_req(uint8_t instance_id, uint8_t type,
			    struct pldm_version_t version,
			    struct pldm_msg_t *msg)
{
	struct pldm_header_info header = {0};
	header.instance = instance_id;
	header.msg_type = REQUEST;
	header.command = PLDM_GET_PLDM_COMMANDS;
	pack_pldm_header(&header, msg);

	uint8_t *dst = msg->payload;
	memcpy(dst, &type, sizeof(type));
	dst += sizeof(type);
	memcpy(dst, &version, sizeof(version));

	return PLDM_SUCCESS;
}

int encode_get_types_resp(uint8_t instance_id, const uint8_t *types,
			  struct pldm_msg_t *msg)
{
	struct pldm_header_info header = {0};
	header.instance = instance_id;
	header.msg_type = RESPONSE;
	header.command = PLDM_GET_PLDM_TYPES;
	pack_pldm_header(&header, msg);

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
	struct pldm_header_info header = {0};
	header.instance = instance_id;
	header.msg_type = RESPONSE;
	header.command = PLDM_GET_PLDM_COMMANDS;
	pack_pldm_header(&header, msg);

	if (msg->payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->payload + sizeof(msg->payload[0]);
		memcpy(dst, commands, PLDM_MAX_CMDS_PER_TYPE / 8);
	}

	return PLDM_SUCCESS;
}

int encode_get_version_req(uint8_t instance_id, uint32_t transfer_handle,
			   uint8_t transfer_opflag, uint8_t type,
			   struct pldm_msg_t *msg)
{
	struct pldm_header_info header = {0};
	int rc;

	if (NULL == msg) {
		return PLDM_ERROR_INVALID_DATA;
	}

	header.msg_type = REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_BASE;
	header.command = PLDM_GET_PLDM_VERSION;

	rc = pack_pldm_header(&header, msg);

	if (rc == PLDM_ERROR_INVALID_DATA) {
		return PLDM_ERROR;
	}

	uint8_t *dst = msg->payload;
	transfer_handle = htole32(transfer_handle);
	memcpy(dst, (uint8_t *)&transfer_handle, sizeof(transfer_handle));
	dst += sizeof(transfer_handle);

	memcpy(dst, &transfer_opflag, sizeof(transfer_opflag));
	dst += sizeof(transfer_opflag);

	memcpy(dst, (uint8_t *)&type, sizeof(type));

	return PLDM_SUCCESS;
}

int encode_get_version_resp(uint8_t instance_id, uint32_t next_transfer_handle,
			    uint8_t transfer_flag,
			    const struct pldm_version_t *version_data,
			    uint32_t version_size, struct pldm_msg_t *msg)
{
	struct pldm_header_info header = {0};
	int rc;

	if (msg->payload[0] == PLDM_SUCCESS) {

		header.msg_type = RESPONSE;
		header.instance = instance_id;
		header.pldm_type = PLDM_BASE;
		header.command = PLDM_GET_PLDM_VERSION;

		rc = pack_pldm_header(&header, msg);

		if (rc == PLDM_ERROR_INVALID_DATA) {
			return PLDM_ERROR;
		}
		uint8_t *dst = msg->payload + sizeof(msg->payload[0]);

		memcpy(dst, &next_transfer_handle,
		       sizeof(next_transfer_handle));
		dst += sizeof(next_transfer_handle);
		memcpy(dst, &transfer_flag, sizeof(transfer_flag));

		dst += sizeof(transfer_flag);
		memcpy((uint32_t *)dst, version_data, version_size);
	}
	return PLDM_SUCCESS;
}

int decode_get_version_req(const struct pldm_msg_t *msg,
			   uint32_t *transfer_handle, uint8_t *transfer_opflag,
			   uint8_t *type)
{
	const uint8_t *start = msg->payload;
	*transfer_handle = *start;
	*transfer_handle = le32toh(*transfer_handle);
	*transfer_opflag = *(start + sizeof(*transfer_handle));
	*type = *(start + sizeof(*transfer_handle) + sizeof(*transfer_opflag));

	return PLDM_SUCCESS;
}

int decode_get_version_resp(const struct pldm_msg_t *msg, uint32_t bufsize,
			    uint32_t *next_transfer_handle,
			    uint8_t *transfer_flag,
			    struct pldm_version_t *version)
{
	const uint8_t *start = msg->payload + sizeof(uint8_t);
	*next_transfer_handle = *start;
	*transfer_flag = *(start + sizeof(*next_transfer_handle));

	struct pldm_version_t *temp =
	    (struct pldm_version_t *)(start + sizeof(*next_transfer_handle) +
				      sizeof(*transfer_flag));

	*version = *temp;

	//*version = (struct pldm_version_t )*(start +
	// sizeof(*next_transfer_handle) + sizeof(*transfer_flag));

	return PLDM_SUCCESS;
}
