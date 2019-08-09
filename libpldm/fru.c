#include <endian.h>
#include <string.h>

#include "fru.h"

int encode_get_fru_record_table_req(uint8_t instance_id,
				    uint32_t data_transfer_handle,
				    uint8_t transfer_operation_flag,
				    struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_FRU;
	header.command = PLDM_GET_FRU_RECORD_TABLE;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct pldm_get_fru_record_table_req *req =
	    (struct pldm_get_fru_record_table_req *)msg->payload;

	req->data_transfer_handle = htole32(data_transfer_handle);
	req->transfer_operation_flag = transfer_operation_flag;

	return PLDM_SUCCESS;
}

int decode_get_fru_record_table_resp(
    const struct pldm_msg *msg, size_t payload_length, uint8_t *completion_code,
    uint32_t *next_data_transfer_handle, uint8_t *transfer_flag,
    uint8_t *fru_record_table_data, size_t *fru_record_table_length)
{
	if (msg == NULL || completion_code == NULL ||
	    next_data_transfer_handle == NULL || transfer_flag == NULL ||
	    fru_record_table_data == NULL || fru_record_table_length == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length <= PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_fru_record_table_resp *resp =
	    (struct pldm_get_fru_record_table_resp *)msg->payload;

	*completion_code = resp->completion_code;

	if (*completion_code == PLDM_SUCCESS) {
		*next_data_transfer_handle =
		    le32toh(resp->next_data_transfer_handle);
		*transfer_flag = resp->transfer_flag;
		memcpy(fru_record_table_data, resp->fru_record_table_data,
		       payload_length -
			   PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES);
		*fru_record_table_length =
		    payload_length - PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES;
	}

	return PLDM_SUCCESS;
}

int encode_get_fru_record_table_resp(
    uint8_t instance_id, uint8_t completion_code,
    uint32_t next_data_transfer_handle, uint8_t transfer_flag,
    uint8_t *fru_record_table_data, size_t payload_length, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_FRU;
	header.command = PLDM_GET_FRU_RECORD_TABLE;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct pldm_get_fru_record_table_resp *resp =
	    (struct pldm_get_fru_record_table_resp *)msg->payload;

	resp->completion_code = completion_code;

	if (resp->completion_code == PLDM_SUCCESS) {

		resp->next_data_transfer_handle =
		    htole32(next_data_transfer_handle);
		resp->transfer_flag = transfer_flag;

		if (fru_record_table_data != NULL &&
		    payload_length >
			(sizeof(struct pldm_msg_hdr) +
			 PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES)) {
			memcpy(resp->fru_record_table_data,
			       fru_record_table_data,
			       payload_length -
				   (sizeof(struct pldm_msg_hdr) +
				    PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES));
		}
	}

	return PLDM_SUCCESS;
}

int decode_get_fru_record_table_req(const struct pldm_msg *msg,
				    size_t payload_length,
				    uint32_t *data_transfer_handle,
				    uint8_t *transfer_operation_flag)
{
	if (msg == NULL || data_transfer_handle == NULL ||
	    transfer_operation_flag == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_fru_record_table_req *req =
	    (struct pldm_get_fru_record_table_req *)msg->payload;

	*data_transfer_handle = le32toh(req->data_transfer_handle);
	*transfer_operation_flag = req->transfer_operation_flag;

	return PLDM_SUCCESS;
}

int encode_get_fru_record_by_option_req(uint8_t instance_id,
					uint32_t data_transfer_handle,
					uint16_t fru_table_handle,
					uint16_t record_set_identifier,
					uint8_t record_type, uint8_t field_type,
					uint8_t transfer_operation_flag,
					struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_FRU;
	header.command = PLDM_GET_FRU_RECORD_BY_OPTION;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct pldm_get_fru_record_by_option_req *req =
	    (struct pldm_get_fru_record_by_option_req *)msg->payload;

	req->data_transfer_handle = htole32(data_transfer_handle);
	req->fru_table_handle = htole16(fru_table_handle);
	req->record_set_identifier = htole16(record_set_identifier);
	req->record_type = record_type;
	req->field_type = field_type;
	req->transfer_operation_flag = transfer_operation_flag;

	return PLDM_SUCCESS;
}

int decode_get_fru_record_by_option_resp(
    const struct pldm_msg *msg, size_t payload_length, uint8_t *completion_code,
    uint32_t *next_data_transfer_handle, uint8_t *transfer_flag,
    uint8_t *fru_data_structure_data, size_t *fru_data_structure_data_length)
{
	if (msg == NULL || completion_code == NULL ||
	    next_data_transfer_handle == NULL || transfer_flag == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length <= PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_fru_record_by_option_resp *resp =
	    (struct pldm_get_fru_record_by_option_resp *)msg->payload;

	*completion_code = resp->completion_code;
	if (*completion_code == PLDM_SUCCESS) {
		*next_data_transfer_handle =
		    le32toh(resp->next_data_transfer_handle);
		*transfer_flag = resp->transfer_flag;
		memcpy(fru_data_structure_data, resp->fru_data_structure_data,
		       payload_length -
			   PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES);
		*fru_data_structure_data_length =
		    payload_length -
		    PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES;
	}

	return PLDM_SUCCESS;
}

int encode_get_fru_record_by_option_resp(uint8_t instance_id,
					 uint8_t completion_code,
					 uint32_t next_data_transfer_handle,
					 uint8_t transfer_flag,
					 uint8_t *fru_data_structure_data,
					 size_t payload_length,
					 struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_FRU;
	header.command = PLDM_GET_FRU_RECORD_BY_OPTION;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct pldm_get_fru_record_by_option_resp *resp =
	    (struct pldm_get_fru_record_by_option_resp *)msg->payload;

	resp->completion_code = completion_code;

	if (resp->completion_code == PLDM_SUCCESS) {

		resp->next_data_transfer_handle =
		    htole32(next_data_transfer_handle);
		resp->transfer_flag = transfer_flag;

		if (fru_data_structure_data != NULL &&
		    payload_length >
			(sizeof(struct pldm_msg_hdr) +
			 PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES)) {
			memcpy(
			    resp->fru_data_structure_data,
			    fru_data_structure_data,
			    payload_length -
				(sizeof(struct pldm_msg_hdr) +
				 PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES));
		}
	}

	return PLDM_SUCCESS;
}
