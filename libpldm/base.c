#include <endian.h>
#include <string.h>

#include "base.h"

int pack_pldm_header(const struct pldm_header_info *hdr,
		     struct pldm_msg_hdr *msg)
{
	if (msg == NULL || hdr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (hdr->msg_type != PLDM_RESPONSE && hdr->msg_type != PLDM_REQUEST &&
	    hdr->msg_type != PLDM_ASYNC_REQUEST_NOTIFY) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (hdr->instance > PLDM_INSTANCE_MAX) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (hdr->pldm_type > (PLDM_MAX_TYPES - 1)) {
		return PLDM_ERROR_INVALID_PLDM_TYPE;
	}

	uint8_t datagram = (hdr->msg_type == PLDM_ASYNC_REQUEST_NOTIFY) ? 1 : 0;

	if (hdr->msg_type == PLDM_RESPONSE) {
		msg->request = PLDM_RESPONSE;
	} else if (hdr->msg_type == PLDM_REQUEST ||
		   hdr->msg_type == PLDM_ASYNC_REQUEST_NOTIFY) {
		msg->request = PLDM_REQUEST;
	}
	msg->datagram = datagram;
	msg->reserved = 0;
	msg->instance_id = hdr->instance;
	msg->header_ver = 0;
	msg->type = hdr->pldm_type;
	msg->command = hdr->command;

	return PLDM_SUCCESS;
}

int unpack_pldm_header(const struct pldm_msg_hdr *msg,
		       struct pldm_header_info *hdr)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (msg->request == PLDM_RESPONSE) {
		hdr->msg_type = PLDM_RESPONSE;
	} else {
		hdr->msg_type =
		    msg->datagram ? PLDM_ASYNC_REQUEST_NOTIFY : PLDM_REQUEST;
	}

	hdr->instance = msg->instance_id;
	hdr->pldm_type = msg->type;
	hdr->command = msg->command;

	return PLDM_SUCCESS;
}

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

int encode_get_version_req(uint8_t instance_id, uint32_t transfer_handle,
			   uint8_t transfer_opflag, uint8_t type,
			   struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};

	if (NULL == msg) {
		return PLDM_ERROR_INVALID_DATA;
	}

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_BASE;
	header.command = PLDM_GET_PLDM_VERSION;

	if (pack_pldm_header(&header, &(msg->hdr)) > PLDM_SUCCESS) {
		return PLDM_ERROR;
	}

	uint8_t *dst = msg->body.payload;
	transfer_handle = htole32(transfer_handle);
	memcpy(dst, &transfer_handle, sizeof(transfer_handle));
	dst += sizeof(transfer_handle);

	memcpy(dst, &transfer_opflag, sizeof(transfer_opflag));
	dst += sizeof(transfer_opflag);

	memcpy(dst, (uint8_t *)&type, sizeof(type));

	return PLDM_SUCCESS;
}

int encode_get_version_resp(uint8_t instance_id, uint8_t completion_code,
			    uint32_t next_transfer_handle,
			    uint8_t transfer_flag,
			    const struct pldm_version *version_data,
			    size_t version_size, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	msg->body.payload[0] = completion_code;
	if (msg->body.payload[0] == PLDM_SUCCESS) {

		header.msg_type = PLDM_RESPONSE;
		header.instance = instance_id;
		header.pldm_type = PLDM_BASE;
		header.command = PLDM_GET_PLDM_VERSION;

		if (pack_pldm_header(&header, &(msg->hdr)) > PLDM_SUCCESS) {
			return PLDM_ERROR;
		}
		uint8_t *dst = msg->body.payload + sizeof(msg->body.payload[0]);

		next_transfer_handle = htole32(next_transfer_handle);

		memcpy(dst, &next_transfer_handle,
		       sizeof(next_transfer_handle));
		dst += sizeof(next_transfer_handle);
		memcpy(dst, &transfer_flag, sizeof(transfer_flag));

		dst += sizeof(transfer_flag);
		memcpy(dst, version_data, version_size);
	}
	return PLDM_SUCCESS;
}

int decode_get_version_req(const struct pldm_msg_payload *msg,
			   uint32_t *transfer_handle, uint8_t *transfer_opflag,
			   uint8_t *type)
{
	const uint8_t *start = msg->payload;
	*transfer_handle = *((uint32_t *)start);
	*transfer_handle = le32toh(*transfer_handle);
	*transfer_opflag = *(start + sizeof(*transfer_handle));
	*type = *(start + sizeof(*transfer_handle) + sizeof(*transfer_opflag));

	return PLDM_SUCCESS;
}

int decode_get_version_resp(const struct pldm_msg_payload *msg,
			    uint32_t *next_transfer_handle,
			    uint8_t *transfer_flag,
			    struct pldm_version *version)
{
	const uint8_t *start = msg->payload + sizeof(uint8_t);
	*next_transfer_handle = *((uint32_t *)start);
	*next_transfer_handle = le32toh(*next_transfer_handle);
	*transfer_flag = *(start + sizeof(*next_transfer_handle));

	*version =
	    *((struct pldm_version *)(start + sizeof(*next_transfer_handle) +
				      sizeof(*transfer_flag)));

	return PLDM_SUCCESS;
}
