#include "file_io.h"
#include <endian.h>
#include <string.h>

int decode_rw_file_memory_req(const struct pldm_msg_payload *msg,
			      uint32_t *fileHandle, uint32_t *offset,
			      uint32_t *length, uint64_t *address)
{
	if (msg == NULL || fileHandle == NULL || offset == NULL ||
	    length == NULL || address == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (msg->payload_length != PLDM_RW_FILE_MEM_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	const uint8_t *start = msg->payload;
	*fileHandle = le32toh(*((uint32_t *)start));
	*offset = le32toh(*((uint32_t *)(start + sizeof(*fileHandle))));
	*length = le32toh(
	    *((uint32_t *)(start + sizeof(*fileHandle) + sizeof(*offset))));
	*address = le64toh(*((uint64_t *)(start + sizeof(*fileHandle) +
					  sizeof(*offset) + sizeof(*length))));

	return PLDM_SUCCESS;
}

int encode_rw_file_memory_resp(uint8_t instance_id, uint8_t command,
			       uint8_t completion_code, uint32_t length,
			       struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_FILE_IO;
	header.command = command;
	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	msg->body.payload[0] = completion_code;
	if (msg->body.payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->body.payload + sizeof(msg->body.payload[0]);
		length = htole32(length);
		memcpy(dst, &length, sizeof(length));
	}

	return PLDM_SUCCESS;
}
