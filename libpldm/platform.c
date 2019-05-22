#include <endian.h>
#include <string.h>

#include "platform.h"

int encode_set_state_effecter_states_resp(uint8_t instance_id,
					  uint8_t completion_code,
					  struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	msg->body.payload[0] = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_SET_STATE_EFFECTER_STATES;

	rc = pack_pldm_header(&header, &(msg->hdr));

	return rc;
}

int encode_set_state_effecter_states_req(uint8_t instance_id,
					 uint16_t effecter_id,
					 uint8_t comp_effecter_count,
					 set_effecter_state_field *field,
					 struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_SET_STATE_EFFECTER_STATES;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (comp_effecter_count < 0x1 || comp_effecter_count > 0x8) {
		return PLDM_ERROR_INVALID_DATA;
	}

	uint8_t *encoded_msg = msg->body.payload;
	effecter_id = htole16(effecter_id);
	memcpy(encoded_msg, &effecter_id, sizeof(effecter_id));
	encoded_msg += sizeof(effecter_id);
	memcpy(encoded_msg, &comp_effecter_count, sizeof(comp_effecter_count));
	encoded_msg += sizeof(comp_effecter_count);
	memcpy(encoded_msg, field,
	       (sizeof(set_effecter_state_field) * comp_effecter_count));

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_resp(const struct pldm_msg_payload *msg,
					  uint8_t *completion_code)
{
	if (msg == NULL || completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = *(uint8_t *)msg->payload;

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_req(const struct pldm_msg_payload *msg,
					 uint16_t *effecter_id,
					 uint8_t *comp_effecter_count,
					 set_effecter_state_field *field)
{
	if (msg == NULL || effecter_id == NULL || comp_effecter_count == NULL ||
	    field == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	const uint8_t *start = msg->payload;
	*effecter_id = le16toh(*((uint16_t *)start));
	*comp_effecter_count = *(start + sizeof(*effecter_id));
	memcpy(field,
	       (start + sizeof(*effecter_id) + sizeof(*comp_effecter_count)),
	       (sizeof(set_effecter_state_field) * (*comp_effecter_count)));

	return PLDM_SUCCESS;
}

int decode_get_pdr_req(const struct pldm_msg_payload *msg,
		       uint32_t *record_hndl, uint32_t *data_transfer_hndl,
		       uint8_t *transfer_op_flag, uint16_t *request_cnt,
		       uint16_t *record_chg_num)
{
	if (msg == NULL || record_hndl == NULL || data_transfer_hndl == NULL ||
	    transfer_op_flag == NULL || request_cnt == NULL ||
	    record_chg_num == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	const uint8_t *start = msg->payload;
	*record_hndl = le32toh(*((uint32_t *)start));
	*data_transfer_hndl =
	    le32toh(*((uint32_t *)(start + sizeof(*record_hndl))));
	*transfer_op_flag =
	    *(start + sizeof(*record_hndl) + sizeof(*data_transfer_hndl));
	*request_cnt = le16toh(*((uint16_t *)(start + sizeof(*record_hndl) +
					      sizeof(*data_transfer_hndl) +
					      sizeof(*transfer_op_flag))));
	*record_chg_num = le16toh(
	    *((uint16_t *)(start + sizeof(*record_hndl) +
			   sizeof(*data_transfer_hndl) +
			   sizeof(*transfer_op_flag) + sizeof(*request_cnt))));

	return PLDM_SUCCESS;
}

int encode_get_pdr_resp(uint8_t instance_id, uint8_t completion_code,
			uint32_t next_record_hndl,
			uint32_t next_data_transfer_hndl, uint8_t transfer_flag,
			uint16_t resp_cnt, const uint8_t *record_data,
			uint8_t transfer_crc, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	msg->body.payload[0] = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_GET_PDR;
	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (msg->body.payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->body.payload + sizeof(msg->body.payload[0]);
		next_record_hndl = htole32(next_record_hndl);
		memcpy(dst, &next_record_hndl, sizeof(next_record_hndl));
		dst += sizeof(next_record_hndl);
		next_data_transfer_hndl = htole32(next_data_transfer_hndl);
		memcpy(dst, &next_data_transfer_hndl,
		       sizeof(next_data_transfer_hndl));
		dst += sizeof(next_data_transfer_hndl);
		memcpy(dst, &transfer_flag, sizeof(transfer_flag));
		dst += sizeof(transfer_flag);
		resp_cnt = htole16(resp_cnt);
		memcpy(dst, &resp_cnt, sizeof(resp_cnt));
		dst += sizeof(resp_cnt);
		memcpy(dst, record_data, resp_cnt);
		dst += resp_cnt;
		memcpy(dst, &transfer_crc, sizeof(transfer_crc));
	}

	return PLDM_SUCCESS;
}

int encode_get_pdr_req(uint8_t instance_id, uint32_t record_handle,
		       uint32_t data_transfer_handle,
		       uint8_t transfer_operation_flag, uint16_t request_count,
		       uint16_t record_change_number, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_GET_PDR;
	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}
	uint8_t *dst = msg->body.payload;
	record_handle = htole32(record_handle);
	memcpy(dst, &record_handle, sizeof(record_handle));
	dst += sizeof(record_handle);
	data_transfer_handle = htole32(data_transfer_handle);
	memcpy(dst, &data_transfer_handle, sizeof(data_transfer_handle));
	dst += sizeof(data_transfer_handle);
	memcpy(dst, &transfer_operation_flag, sizeof(transfer_operation_flag));
	dst += sizeof(transfer_operation_flag);
	request_count = htole16(request_count);
	memcpy(dst, &request_count, sizeof(request_count));
	dst += sizeof(request_count);
	record_change_number = htole16(record_change_number);
	memcpy(dst, &record_change_number, sizeof(record_change_number));
	return PLDM_SUCCESS;
}

int decode_get_pdr_resp(const struct pldm_msg_payload *msg,
			uint8_t *completion_code, uint32_t *next_record_handle,
			uint32_t *next_data_transfer_handle,
			uint8_t *transfer_flag, uint16_t *response_count,
			uint8_t *record_data, uint8_t *transfer_crc)
{
	if (msg == NULL || completion_code == NULL ||
	    next_record_handle == NULL || next_data_transfer_handle == NULL ||
	    transfer_flag == NULL || response_count == NULL ||
	    record_data == NULL || transfer_crc == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	const uint8_t *start = msg->payload;
	*completion_code = *start;
	if (*completion_code == PLDM_SUCCESS) {
		*next_record_handle =
		    le32toh(*(uint32_t *)(start + sizeof(*completion_code)));
		*next_data_transfer_handle =
		    le32toh(*(uint32_t *)(start + sizeof(*completion_code) +
					  sizeof(*next_record_handle)));
		*transfer_flag = *(start + sizeof(*completion_code) +
				   sizeof(*next_record_handle) +
				   sizeof(*next_data_transfer_handle));
		*response_count =
		    le16toh(*(uint16_t *)(start + sizeof(*completion_code) +
					  sizeof(*next_record_handle) +
					  sizeof(*next_data_transfer_handle) +
					  sizeof(*transfer_flag)));
		memcpy(record_data,
		       start + sizeof(*completion_code) +
			   sizeof(*next_record_handle) +
			   sizeof(*next_data_transfer_handle) +
			   sizeof(*transfer_flag) + sizeof(*response_count),
		       *response_count);
		*transfer_crc = *(start + sizeof(*completion_code) +
				  sizeof(*next_record_handle) +
				  sizeof(*next_data_transfer_handle) +
				  sizeof(*transfer_flag) +
				  sizeof(*response_count) + *response_count);
	}
	return PLDM_SUCCESS;
}
