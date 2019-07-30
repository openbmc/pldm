#include <endian.h>
#include <string.h>

#include "bios.h"

int encode_get_date_time_req(uint8_t instance_id, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_BIOS;
	header.command = PLDM_GET_DATE_TIME;
	return pack_pldm_header(&header, &(msg->hdr));
}

int encode_get_date_time_resp(uint8_t instance_id, uint8_t completion_code,
			      uint8_t seconds, uint8_t minutes, uint8_t hours,
			      uint8_t day, uint8_t month, uint16_t year,
			      struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_BIOS;
	header.command = PLDM_GET_DATE_TIME;

	struct pldm_get_date_time_resp *response =
	    (struct pldm_get_date_time_resp *)msg->payload;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	response->completion_code = completion_code;
	if (response->completion_code == PLDM_SUCCESS) {
		response->completion_code = completion_code;
		response->seconds = seconds;
		response->minutes = minutes;
		response->hours = hours;
		response->day = day;
		response->month = month;
		response->year = htole16(year);
	}
	return PLDM_SUCCESS;
}

int decode_get_date_time_resp(const struct pldm_msg *msg, size_t payload_length,
			      uint8_t *completion_code, uint8_t *seconds,
			      uint8_t *minutes, uint8_t *hours, uint8_t *day,
			      uint8_t *month, uint16_t *year)
{
	if (msg == NULL || seconds == NULL || minutes == NULL ||
	    hours == NULL || day == NULL || month == NULL || year == NULL ||
	    completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_GET_DATE_TIME_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_date_time_resp *response =
	    (struct pldm_get_date_time_resp *)msg->payload;
	*completion_code = response->completion_code;

	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	}
	*seconds = response->seconds;
	*minutes = response->minutes;
	*hours = response->hours;
	*day = response->day;
	*month = response->month;
	*year = le16toh(response->year);

	return PLDM_SUCCESS;
}

int encode_get_bios_table_resp(uint8_t instance_id, uint8_t completion_code,
			       uint32_t next_transfer_handle,
			       uint8_t transfer_flag, uint8_t *table_data,
			       size_t payload_length, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct pldm_get_bios_table_resp *response =
	    (struct pldm_get_bios_table_resp *)msg->payload;

	response->completion_code = completion_code;
	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_BIOS;
	header.command = PLDM_GET_BIOS_TABLE;
	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (response->completion_code == PLDM_SUCCESS) {

		response->next_transfer_handle = htole32(next_transfer_handle);
		response->transfer_flag = transfer_flag;
		if (table_data != NULL &&
		    payload_length > (sizeof(struct pldm_msg_hdr) +
				      PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES)) {
			memcpy(response->table_data, table_data,
			       payload_length -
				   (sizeof(struct pldm_msg_hdr) +
				    PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES));
		}
	}
	return PLDM_SUCCESS;
}

int decode_get_bios_table_req(const struct pldm_msg *msg, size_t payload_length,
			      uint32_t *transfer_handle,
			      uint8_t *transfer_op_flag, uint8_t *table_type)
{
	if (msg == NULL || transfer_op_flag == NULL || table_type == NULL ||
	    transfer_handle == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_GET_BIOS_TABLE_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_bios_table_req *request =
	    (struct pldm_get_bios_table_req *)msg->payload;
	*transfer_handle = le32toh(request->transfer_handle);
	*transfer_op_flag = request->transfer_op_flag;
	*table_type = request->table_type;

	return PLDM_SUCCESS;
}

int decode_get_bios_attribute_current_value_by_handle_req(
    const struct pldm_msg *msg, size_t payload_length,
    uint32_t *transfer_handle, uint8_t *transfer_op_flag,
    uint16_t *attribute_handle)
{
	if (msg == NULL || transfer_handle == NULL ||
	    transfer_op_flag == NULL || attribute_handle == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_GET_BIOS_ATTR_CURR_VAL_BY_HANDLE_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_bios_attribute_current_value_by_handle_req *request =
	    (struct pldm_get_bios_attribute_current_value_by_handle_req *)
		msg->payload;
	*transfer_handle = le32toh(request->transfer_handle);
	*transfer_op_flag = request->transfer_op_flag;
	*attribute_handle = le16toh(request->attribute_handle);

	return PLDM_SUCCESS;
}

int encode_get_bios_current_value_by_handle_resp(
    uint8_t instance_id, uint8_t completion_code, uint32_t next_transfer_handle,
    uint8_t transfer_flag, const uint8_t *attribute_data,
    size_t attribute_length, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL || attribute_data == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct pldm_get_bios_attribute_current_value_by_handle_resp *response =
	    (struct pldm_get_bios_attribute_current_value_by_handle_resp *)
		msg->payload;

	response->completion_code = completion_code;
	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_BIOS;
	header.command = PLDM_GET_BIOS_ATTRIBUTE_CURRENT_VALUE_BY_HANDLE;
	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}
	if (response->completion_code == PLDM_SUCCESS) {

		response->next_transfer_handle = htole32(next_transfer_handle);
		response->transfer_flag = transfer_flag;
		if (attribute_data != NULL) {
			memcpy(response->attribute_data, attribute_data,
			       attribute_length);
		}
	}
	return PLDM_SUCCESS;
}
