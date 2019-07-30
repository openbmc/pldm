#include <endian.h>
#include <string.h>

#include "fru.h"

int encode_get_fru_record_table_req(uint8_t instance_id,
				    uint32_t data_transfer_handle,
				    uint8_t transfer_operation_flag,
				    struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_ERROR_INVALID_DATA;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_FRU;
	header.command = PLDM_GET_FRU_RECORD_TABLE;

	if (msg == NULL) {
		return rc;
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
	int rc = PLDM_ERROR_INVALID_DATA;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_FRU;
	header.command = PLDM_GET_FRU_RECORD_TABLE;

	if (msg == NULL) {
		return rc;
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
