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

	struct pldm_header_info header = {0};
	header.instance = instance_id;
	header.msg_type = PLDM_REQUEST;
	header.command = PLDM_GET_PLDM_TYPES;
	pack_pldm_header(&header, &(msg->hdr));

	return PLDM_SUCCESS;
}

int encode_get_commands_req(uint8_t instance_id, uint8_t type, ver32_t version,
			    struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct pldm_header_info header = {0};
	header.instance = instance_id;
	header.msg_type = PLDM_REQUEST;
	header.command = PLDM_GET_PLDM_COMMANDS;
	pack_pldm_header(&header, &(msg->hdr));

	struct PLDM_GetCommands_Request *request =
	    (struct PLDM_GetCommands_Request *)msg->body.payload;

	request->type = type;
	request->version = version;

	return PLDM_SUCCESS;
}

int encode_get_types_resp(uint8_t instance_id, uint8_t completion_code,
			  const bitfield8_t *types, struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct PLDM_GetTypes_Response *response =
	    (struct PLDM_GetTypes_Response *)msg->body.payload;

	response->completion_code = completion_code;
	struct pldm_header_info header = {0};
	header.instance = instance_id;
	header.msg_type = PLDM_RESPONSE;
	header.command = PLDM_GET_PLDM_TYPES;
	pack_pldm_header(&header, &(msg->hdr));

	if (response->completion_code == PLDM_SUCCESS) {
		if (types == NULL) {
			return PLDM_ERROR_INVALID_DATA;
		}
		memcpy(response->types, &(types->byte), PLDM_MAX_TYPES / 8);
	}

	return PLDM_SUCCESS;
}

int decode_get_commands_req(const struct pldm_msg_payload *msg, uint8_t *type,
			    ver32_t *version)
{
	if (msg == NULL || type == NULL || version == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	struct PLDM_GetCommands_Request *start =
	    (struct PLDM_GetCommands_Request *)msg->payload;
	*type = start->type;
	*version = start->version;
	return PLDM_SUCCESS;
}

int encode_get_commands_resp(uint8_t instance_id, uint8_t completion_code,
			     const bitfield8_t *commands, struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct PLDM_GetCommands_Response *dst =
	    (struct PLDM_GetCommands_Response *)msg->body.payload;
	dst->completion_code = completion_code;

	struct pldm_header_info header = {0};
	header.instance = instance_id;
	header.msg_type = PLDM_RESPONSE;
	header.command = PLDM_GET_PLDM_COMMANDS;
	pack_pldm_header(&header, &(msg->hdr));

	if (dst->completion_code == PLDM_SUCCESS) {
		if (commands == NULL) {
			return PLDM_ERROR_INVALID_DATA;
		}
		memcpy(dst->commands, &(commands->byte),
		       PLDM_MAX_CMDS_PER_TYPE / 8);
	}

	return PLDM_SUCCESS;
}

int decode_get_types_resp(const struct pldm_msg_payload *msg,
			  uint8_t *completion_code, bitfield8_t *types)
{
	if (msg == NULL || types == NULL || msg->payload == NULL ||
	    completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct PLDM_GetTypes_Response *src =
	    (struct PLDM_GetTypes_Response *)msg->payload;
	*completion_code = src->completion_code;
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	};

	memcpy(&(types->byte), src->types, PLDM_MAX_TYPES / 8);

	return PLDM_SUCCESS;
}

int decode_get_commands_resp(const struct pldm_msg_payload *msg,
			     uint8_t *completion_code, bitfield8_t *commands)
{
	if (msg == NULL || commands == NULL || msg->payload == NULL ||
	    completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	struct PLDM_GetCommands_Response *src =
	    (struct PLDM_GetCommands_Response *)msg->payload;
	*completion_code = src->completion_code;
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	};

	memcpy(&(commands->byte), src->commands, PLDM_MAX_CMDS_PER_TYPE / 8);

	return PLDM_SUCCESS;
}

int encode_get_version_req(uint8_t instance_id, uint32_t transfer_handle,
			   uint8_t transfer_opflag, uint8_t type,
			   struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (NULL == msg) {
		return PLDM_ERROR_INVALID_DATA;
	}

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_BASE;
	header.command = PLDM_GET_PLDM_VERSION;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct PLDM_GetVersion_Request *req =
	    (struct PLDM_GetVersion_Request *)msg->body.payload;
	transfer_handle = htole32(transfer_handle);
	req->transfer_handle = transfer_handle;
	req->transfer_opflag = transfer_opflag;
	req->type = type;

	return PLDM_SUCCESS;
}

int encode_get_version_resp(uint8_t instance_id, uint8_t completion_code,
			    uint32_t next_transfer_handle,
			    uint8_t transfer_flag, const ver32_t *version_data,
			    size_t version_size, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;
	struct PLDM_GetVersion_Response *dst =
	    (struct PLDM_GetVersion_Response *)msg->body.payload;
	dst->completion_code = completion_code;
	if (dst->completion_code == PLDM_SUCCESS) {

		header.msg_type = PLDM_RESPONSE;
		header.instance = instance_id;
		header.pldm_type = PLDM_BASE;
		header.command = PLDM_GET_PLDM_VERSION;

		if ((rc = pack_pldm_header(&header, &(msg->hdr))) >
		    PLDM_SUCCESS) {
			return rc;
		}

		next_transfer_handle = htole32(next_transfer_handle);
		dst->next_transfer_handle = next_transfer_handle;
		dst->transfer_flag = transfer_flag;
		memcpy(dst->version_data, (uint8_t *)version_data,
		       version_size);
	}
	return PLDM_SUCCESS;
}

int decode_get_version_req(const struct pldm_msg_payload *msg,
			   uint32_t *transfer_handle, uint8_t *transfer_opflag,
			   uint8_t *type)
{
	struct PLDM_GetVersion_Request *start =
	    (struct PLDM_GetVersion_Request *)msg->payload;
	*transfer_handle = le32toh(start->transfer_handle);
	*transfer_opflag = start->transfer_opflag;
	*type = start->type;
	return PLDM_SUCCESS;
}

int decode_get_version_resp(const struct pldm_msg_payload *msg,
			    uint8_t *completion_code,
			    uint32_t *next_transfer_handle,
			    uint8_t *transfer_flag, ver32_t *version)
{
	if (msg == NULL || next_transfer_handle == NULL ||
	    transfer_flag == NULL || msg->payload == NULL ||
	    completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	struct PLDM_GetVersion_Response *start =
	    (struct PLDM_GetVersion_Response *)msg->payload;
	*completion_code = start->completion_code;
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	};

	*next_transfer_handle = le32toh(start->next_transfer_handle);
	*transfer_flag = start->transfer_flag;
	memcpy(version, (uint8_t *)start->version_data, sizeof(ver32_t));

	return PLDM_SUCCESS;
}
