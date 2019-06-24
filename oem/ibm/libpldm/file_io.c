#include "file_io.h"
#include <endian.h>
#include <string.h>

int decode_rw_file_memory_req(const uint8_t *msg, size_t payload_length,
			      uint32_t *file_handle, uint32_t *offset,
			      uint32_t *length, uint64_t *address)
{
	if (msg == NULL || file_handle == NULL || offset == NULL ||
	    length == NULL || address == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_RW_FILE_MEM_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*file_handle = le32toh(*((uint32_t *)msg));
	*offset = le32toh(*((uint32_t *)(msg + sizeof(*file_handle))));
	*length = le32toh(
	    *((uint32_t *)(msg + sizeof(*file_handle) + sizeof(*offset))));
	*address = le64toh(*((uint64_t *)(msg + sizeof(*file_handle) +
					  sizeof(*offset) + sizeof(*length))));

	return PLDM_SUCCESS;
}

int encode_rw_file_memory_resp(uint8_t instance_id, uint8_t command,
			       uint8_t completion_code, uint32_t length,
			       struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	uint8_t *payload = msg->payload;
	*payload = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_IBM_OEM_TYPE;
	header.command = command;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (msg->payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->payload + sizeof(completion_code);
		length = htole32(length);
		memcpy(dst, &length, sizeof(length));
	}

	return PLDM_SUCCESS;
}

int decode_read_file_req(const uint8_t *msg, size_t payload_length,
			 uint32_t *file_handle, uint32_t *offset,
			 uint32_t *length)
{
	if (msg == NULL || file_handle == NULL || offset == NULL ||
	    length == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_READ_FILE_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*file_handle = le32toh(*((uint32_t *)msg));
	*offset = le32toh(*((uint32_t *)(msg + sizeof(*file_handle))));
	*length = le32toh(
	    *((uint32_t *)(msg + sizeof(*file_handle) + sizeof(*offset))));

	return PLDM_SUCCESS;
}

int encode_read_file_resp(uint8_t instance_id, uint8_t command,
			  uint8_t completion_code, uint32_t length,
			  uint8_t *data, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	uint8_t *payload = msg->payload;
	*payload = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_IBM_OEM_TYPE;
	header.command = command;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (msg->payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->payload + sizeof(completion_code);
		length = htole32(length);
		memcpy(dst, &length, sizeof(length));
		dst += sizeof(length);
		memcpy(dst, data, length);
	}

	return PLDM_SUCCESS;
}

int decode_write_file_req(const uint8_t *msg, size_t payload_length,
			  uint32_t *file_handle, uint32_t *offset,
			  uint32_t *length, uint8_t *data)
{
	if (msg == NULL || file_handle == NULL || offset == NULL ||
	    length == NULL || data == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length !=
	    PLDM_CONST_WRITE_FILE_REQ_BYTES + PLDM_MAX_FILE_SIZE) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*file_handle = le32toh(*((uint32_t *)msg));
	*offset = le32toh(*((uint32_t *)(msg + sizeof(*file_handle))));
	*length = le32toh(
	    *((uint32_t *)(msg + sizeof(*file_handle) + sizeof(*offset))));
	memcpy(data,
	       (msg + sizeof(*file_handle) + sizeof(*offset) + sizeof(*length)),
	       *length);

	return PLDM_SUCCESS;
}

int encode_write_file_resp(uint8_t instance_id, uint8_t command,
			   uint8_t completion_code, uint32_t length,
			   struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	uint8_t *payload = msg->payload;
	*payload = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_IBM_OEM_TYPE;
	header.command = command;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (msg->payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->payload + sizeof(completion_code);
		length = htole32(length);
		memcpy(dst, &length, sizeof(length));
	}

	return PLDM_SUCCESS;
}
