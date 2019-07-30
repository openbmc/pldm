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

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct pldm_get_fru_record_table_req *req =
	    (struct pldm_get_fru_record_table_req *)msg->payload;

	req->data_transfer_handle = htole32(data_transfer_handle);
	req->transfer_operation_flag = transfer_operation_flag;

	return PLDM_SUCCESS;
}

int decode_get_fru_record_table_resp(const struct pldm_msg *msg,
				     size_t payload_length,
				     uint8_t *completion_code,
				     uint32_t *next_data_transfer_handle,
				     uint8_t *transfer_flag,
				     size_t *fru_record_offset)
{
	if (msg == NULL || completion_code == NULL ||
	    next_data_transfer_handle == NULL || transfer_flag == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	if (payload_length < PLDM_GET_FRU_RECORD_TABLE_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_fru_record_table_resp *resp =
	    (struct pldm_get_fru_record_table_resp *)msg->payload;

	*completion_code = resp->completion_code;
	if (*completion_code == PLDM_SUCCESS) {
		*next_data_transfer_handle =
		    le32toh(resp->next_data_transfer_handle);
		*transfer_flag = resp->transfer_flag;
		if (payload_length != PLDM_GET_FRU_RECORD_TABLE_RESP_BYTES +
					  *next_data_transfer_handle +
					  *transfer_flag) {
			return PLDM_ERROR_INVALID_LENGTH;
		}

		*fru_record_offset = sizeof(*completion_code) +
				     sizeof(*next_data_transfer_handle) +
				     sizeof(*transfer_flag);
	}

	return PLDM_SUCCESS;
}

int encode_get_fru_record_table_resp(uint8_t instance_id,
				     uint8_t completion_code,
				     uint32_t next_data_transfer_handle,
				     uint8_t transfer_flag,
				     struct pldm_msg *msg)
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

	if (payload_length < PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_fru_record_table_req *req =
	    (struct pldm_get_fru_record_table_req *)msg->payload;

	*data_transfer_handle = le32toh(req->data_transfer_handle);
	*transfer_operation_flag = req->transfer_operation_flag;

	return PLDM_SUCCESS;
}
