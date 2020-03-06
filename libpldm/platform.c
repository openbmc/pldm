#include <endian.h>
#include <string.h>

#include "platform.h"

int encode_set_state_effecter_states_resp(uint8_t instance_id,
					  uint8_t completion_code,
					  struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	msg->payload[0] = completion_code;

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

	struct pldm_set_state_effecter_states_req *request =
	    (struct pldm_set_state_effecter_states_req *)msg->payload;
	effecter_id = htole16(effecter_id);
	request->effecter_id = effecter_id;
	request->comp_effecter_count = comp_effecter_count;
	memcpy(request->field, field,
	       (sizeof(set_effecter_state_field) * comp_effecter_count));

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_resp(const struct pldm_msg *msg,
					  size_t payload_length,
					  uint8_t *completion_code)
{
	if (msg == NULL || completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg->payload[0];
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	}

	if (payload_length > PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	return PLDM_SUCCESS;
}

int decode_set_state_effecter_states_req(const struct pldm_msg *msg,
					 size_t payload_length,
					 uint16_t *effecter_id,
					 uint8_t *comp_effecter_count,
					 set_effecter_state_field *field)
{
	if (msg == NULL || effecter_id == NULL || comp_effecter_count == NULL ||
	    field == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length > PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_set_state_effecter_states_req *request =
	    (struct pldm_set_state_effecter_states_req *)msg->payload;

	*effecter_id = le16toh(request->effecter_id);
	*comp_effecter_count = request->comp_effecter_count;
	memcpy(field, request->field,
	       (sizeof(set_effecter_state_field) * (*comp_effecter_count)));

	return PLDM_SUCCESS;
}

int decode_get_pdr_req(const struct pldm_msg *msg, size_t payload_length,
		       uint32_t *record_hndl, uint32_t *data_transfer_hndl,
		       uint8_t *transfer_op_flag, uint16_t *request_cnt,
		       uint16_t *record_chg_num)
{
	if (msg == NULL || record_hndl == NULL || data_transfer_hndl == NULL ||
	    transfer_op_flag == NULL || request_cnt == NULL ||
	    record_chg_num == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	if (payload_length != PLDM_GET_PDR_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_pdr_req *request =
	    (struct pldm_get_pdr_req *)msg->payload;
	*record_hndl = le32toh(request->record_handle);
	*data_transfer_hndl = le32toh(request->data_transfer_handle);
	*transfer_op_flag = request->transfer_op_flag;
	*request_cnt = le16toh(request->request_count);
	*record_chg_num = le16toh(request->record_change_number);

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
	struct pldm_get_pdr_resp *response =
	    (struct pldm_get_pdr_resp *)msg->payload;

	response->completion_code = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_GET_PDR;
	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (response->completion_code == PLDM_SUCCESS) {
		response->next_record_handle = htole32(next_record_hndl);
		response->next_data_transfer_handle =
		    htole32(next_data_transfer_hndl);
		response->transfer_flag = transfer_flag;
		response->response_count = htole16(resp_cnt);
		if (record_data != NULL && resp_cnt > 0) {
			memcpy(response->record_data, record_data, resp_cnt);
		}
		if (transfer_flag == PLDM_END) {
			uint8_t *dst = msg->payload;
			dst +=
			    (sizeof(struct pldm_get_pdr_resp) - 1) + resp_cnt;
			*dst = transfer_crc;
		}
	}

	return PLDM_SUCCESS;
}

int encode_get_pdr_req(uint8_t instance_id, uint32_t record_hndl,
		       uint32_t data_transfer_hndl, uint8_t transfer_op_flag,
		       uint16_t request_cnt, uint16_t record_chg_num,
		       struct pldm_msg *msg, size_t payload_length)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	struct pldm_get_pdr_req *request =
	    (struct pldm_get_pdr_req *)msg->payload;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_GET_PDR;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (payload_length != PLDM_GET_PDR_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	request->record_handle = htole32(record_hndl);
	request->data_transfer_handle = htole32(data_transfer_hndl);
	request->transfer_op_flag = transfer_op_flag;
	request->request_count = htole16(request_cnt);
	request->record_change_number = htole16(record_chg_num);

	return PLDM_SUCCESS;
}

int decode_get_pdr_resp(const struct pldm_msg *msg, size_t payload_length,
			uint8_t *completion_code, uint32_t *next_record_hndl,
			uint32_t *next_data_transfer_hndl,
			uint8_t *transfer_flag, uint16_t *resp_cnt,
			uint8_t *record_data, size_t record_data_length,
			uint8_t *transfer_crc)
{
	if (msg == NULL || completion_code == NULL ||
	    next_record_hndl == NULL || next_data_transfer_hndl == NULL ||
	    transfer_flag == NULL || resp_cnt == NULL || record_data == NULL ||
	    transfer_crc == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg->payload[0];
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	}

	if (payload_length < PLDM_GET_PDR_MIN_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_pdr_resp *response =
	    (struct pldm_get_pdr_resp *)msg->payload;

	*next_record_hndl = le32toh(response->next_record_handle);
	*next_data_transfer_hndl = le32toh(response->next_data_transfer_handle);
	*transfer_flag = response->transfer_flag;
	*resp_cnt = le16toh(response->response_count);

	if (*transfer_flag != PLDM_END &&
	    (int)payload_length != PLDM_GET_PDR_MIN_RESP_BYTES + *resp_cnt) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	if (*transfer_flag == PLDM_END &&
	    (int)payload_length !=
		PLDM_GET_PDR_MIN_RESP_BYTES + *resp_cnt + 1) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	if (*resp_cnt > 0) {
		if (record_data_length < *resp_cnt) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		memcpy(record_data, response->record_data, *resp_cnt);
	}

	if (*transfer_flag == PLDM_END) {
		*transfer_crc =
		    msg->payload[PLDM_GET_PDR_MIN_RESP_BYTES + *resp_cnt];
	}

	return PLDM_SUCCESS;
}

int decode_set_numeric_effecter_value_req(const struct pldm_msg *msg,
					  size_t payload_length,
					  uint16_t *effecter_id,
					  uint8_t *effecter_data_size,
					  uint8_t *effecter_value)
{
	if (msg == NULL || effecter_id == NULL || effecter_data_size == NULL ||
	    effecter_value == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length < PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_set_numeric_effecter_value_req *request =
	    (struct pldm_set_numeric_effecter_value_req *)msg->payload;
	*effecter_id = le16toh(request->effecter_id);
	*effecter_data_size = request->effecter_data_size;

	if (*effecter_data_size > PLDM_EFFECTER_DATA_SIZE_SINT32) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (*effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT8 ||
	    *effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT8) {

		if (payload_length !=
		    PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES) {
			return PLDM_ERROR_INVALID_LENGTH;
		}

		*effecter_value = request->effecter_value[0];
	}

	if (*effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT16 ||
	    *effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT16) {

		if (payload_length !=
		    PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 1) {
			return PLDM_ERROR_INVALID_LENGTH;
		}

		memcpy(effecter_value, request->effecter_value, 2);
		*effecter_value = le16toh(*effecter_value);
	}

	if (*effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT32 ||
	    *effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT32) {

		if (payload_length !=
		    PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 3) {
			return PLDM_ERROR_INVALID_LENGTH;
		}

		memcpy(effecter_value, request->effecter_value, 4);
	}

	return PLDM_SUCCESS;
}

int encode_set_numeric_effecter_value_resp(uint8_t instance_id,
					   uint8_t completion_code,
					   struct pldm_msg *msg,
					   size_t payload_length)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_SET_NUMERIC_EFFECTER_VALUE_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	msg->payload[0] = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_SET_NUMERIC_EFFECTER_VALUE;

	rc = pack_pldm_header(&header, &(msg->hdr));

	return rc;
}

int encode_set_numeric_effecter_value_req(
    uint8_t instance_id, uint16_t effecter_id, uint8_t effecter_data_size,
    uint8_t *effecter_value, struct pldm_msg *msg, size_t payload_length)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL || effecter_value == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	struct pldm_set_numeric_effecter_value_req *request =
	    (struct pldm_set_numeric_effecter_value_req *)msg->payload;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_SET_NUMERIC_EFFECTER_VALUE;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (effecter_data_size > PLDM_EFFECTER_DATA_SIZE_SINT32) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT8 ||
	    effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT8) {
		if (payload_length !=
		    PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		request->effecter_value[0] = *effecter_value;
	} else if (effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT16 ||
		   effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT16) {
		if (payload_length !=
		    PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 1) {
			return PLDM_ERROR_INVALID_LENGTH;
		}

		memcpy(request->effecter_value, effecter_value, 2);
		*request->effecter_value = htole16(*request->effecter_value);
	} else if (effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT32 ||
		   effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT32) {
		if (payload_length !=
		    PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES + 3) {
			return PLDM_ERROR_INVALID_LENGTH;
		}

		memcpy(request->effecter_value, effecter_value, 4);
		*request->effecter_value = htole32(*request->effecter_value);
	}

	request->effecter_id = htole16(effecter_id);
	request->effecter_data_size = effecter_data_size;

	return PLDM_SUCCESS;
}

int decode_set_numeric_effecter_value_resp(const struct pldm_msg *msg,
					   size_t payload_length,
					   uint8_t *completion_code)
{
	if (msg == NULL || completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_SET_NUMERIC_EFFECTER_VALUE_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*completion_code = msg->payload[0];

	return PLDM_SUCCESS;
}

int encode_get_state_sensor_readings_resp(uint8_t instance_id,
					  uint8_t completion_code,
					  uint8_t comp_sensor_count,
					  get_sensor_state_field *field,
					  struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_GET_STATE_SENSOR_READINGS;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (comp_sensor_count < 0x1 || comp_sensor_count > 0x8) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct pldm_get_state_sensor_readings_resp *response =
	    (struct pldm_get_state_sensor_readings_resp *)msg->payload;

	response->completion_code = completion_code;
	response->comp_sensor_count = comp_sensor_count;
	memcpy(response->field, field,
	       (sizeof(get_sensor_state_field) * comp_sensor_count));

	return rc;
}

int encode_get_state_sensor_readings_req(uint8_t instance_id,
					 uint16_t sensor_id,
					 bitfield8_t sensor_rearm,
					 uint8_t reserved, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_GET_STATE_SENSOR_READINGS;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct pldm_get_state_sensor_readings_req *request =
	    (struct pldm_get_state_sensor_readings_req *)msg->payload;

	request->sensor_id = htole16(sensor_id);
	request->reserved = reserved;
	request->sensor_rearm = sensor_rearm;

	return PLDM_SUCCESS;
}

int decode_get_state_sensor_readings_resp(const struct pldm_msg *msg,
					  size_t payload_length,
					  uint8_t *completion_code,
					  uint8_t *comp_sensor_count,
					  get_sensor_state_field *field)
{
	if (msg == NULL || completion_code == NULL ||
	    comp_sensor_count == NULL || field == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg->payload[0];
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	}

	if (payload_length > PLDM_GET_STATE_SENSOR_READINGS_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_state_sensor_readings_resp *response =
	    (struct pldm_get_state_sensor_readings_resp *)msg->payload;

	if (response->comp_sensor_count < 0x1 ||
	    response->comp_sensor_count > 0x8) {
		return PLDM_ERROR_INVALID_DATA;
	}
	if (response->comp_sensor_count > *comp_sensor_count) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	*comp_sensor_count = response->comp_sensor_count;

	memcpy(field, response->field,
	       (sizeof(get_sensor_state_field) * (*comp_sensor_count)));

	return PLDM_SUCCESS;
}

int decode_get_state_sensor_readings_req(const struct pldm_msg *msg,
					 size_t payload_length,
					 uint16_t *sensor_id,
					 bitfield8_t *sensor_rearm,
					 uint8_t *reserved)
{
	if (msg == NULL || sensor_id == NULL || sensor_rearm == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_GET_STATE_SENSOR_READINGS_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_state_sensor_readings_req *request =
	    (struct pldm_get_state_sensor_readings_req *)msg->payload;

	*sensor_id = le16toh(request->sensor_id);
	*reserved = request->reserved;
	memcpy(&(sensor_rearm->byte), &(request->sensor_rearm.byte),
	       sizeof(request->sensor_rearm.byte));

	return PLDM_SUCCESS;
}

int encode_get_numeric_effecter_value_req(uint8_t instance_id,
					  uint16_t effecter_id,
					  struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct pldm_get_numeric_effecter_value_req *request =
	    (struct pldm_get_numeric_effecter_value_req *)msg->payload;

	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_GET_NUMERIC_EFFECTER_VALUE;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	request->effecter_id = htole16(effecter_id);

	return PLDM_SUCCESS;
}

int encode_get_numeric_effecter_value_resp(
    uint8_t instance_id, uint8_t completion_code, uint8_t effecter_data_size,
    uint8_t effecter_oper_state, uint8_t *pending_value, uint8_t *present_value,
    struct pldm_msg *msg, size_t payload_length)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL || pending_value == NULL || present_value == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (effecter_data_size > PLDM_EFFECTER_DATA_SIZE_SINT32) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (effecter_oper_state > EFFECTER_OPER_STATE_INTEST) {
		return PLDM_ERROR_INVALID_DATA;
	}

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_PLATFORM;
	header.command = PLDM_GET_NUMERIC_EFFECTER_VALUE;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct pldm_get_numeric_effecter_value_resp *response =
	    (struct pldm_get_numeric_effecter_value_resp *)msg->payload;

	response->completion_code = completion_code;
	response->effecter_data_size = effecter_data_size;
	response->effecter_oper_state = effecter_oper_state;

	if (effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT8 ||
	    effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT8) {
		if (payload_length !=
		    PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		response->pending_and_present_values[0] = *pending_value;
		response->pending_and_present_values[1] = *present_value;

	} else if (effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT16 ||
		   effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT16) {
		if (payload_length !=
		    PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES + 2) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		memcpy(response->pending_and_present_values, pending_value, 2);
		memcpy(&response->pending_and_present_values[2], present_value,
		       2);
		*response->pending_and_present_values =
		    htole16(*response->pending_and_present_values);
		response->pending_and_present_values[2] =
		    htole16(response->pending_and_present_values[2]);

	} else if (effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT32 ||
		   effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT32) {
		if (payload_length !=
		    PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES + 6) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		memcpy(response->pending_and_present_values, pending_value, 4);
		memcpy(&response->pending_and_present_values[4], present_value,
		       4);
		*response->pending_and_present_values =
		    htole32(*response->pending_and_present_values);
		response->pending_and_present_values[4] =
		    htole32(response->pending_and_present_values[4]);
	}

	return PLDM_SUCCESS;
}

int decode_get_numeric_effecter_value_req(const struct pldm_msg *msg,
					  size_t payload_length,
					  uint16_t *effecter_id)
{
	if (msg == NULL || effecter_id == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_GET_NUMERIC_EFFECTER_VALUE_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_numeric_effecter_value_req *request =
	    (struct pldm_get_numeric_effecter_value_req *)msg->payload;

	*effecter_id = le16toh(request->effecter_id);

	return PLDM_SUCCESS;
}

int decode_get_numeric_effecter_value_resp(
    const struct pldm_msg *msg, size_t payload_length, uint8_t *completion_code,
    uint8_t *effecter_data_size, uint8_t *effecter_oper_state,
    uint8_t *pending_value, uint8_t *present_value)
{
	if (msg == NULL || effecter_data_size == NULL ||
	    effecter_oper_state == NULL || pending_value == NULL ||
	    present_value == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg->payload[0];
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	}

	if (payload_length < PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_numeric_effecter_value_resp *response =
	    (struct pldm_get_numeric_effecter_value_resp *)msg->payload;

	*effecter_data_size = response->effecter_data_size;
	*effecter_oper_state = response->effecter_oper_state;

	if (*effecter_data_size > PLDM_EFFECTER_DATA_SIZE_SINT32) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (*effecter_oper_state > EFFECTER_OPER_STATE_INTEST) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (*effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT8 ||
	    *effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT8) {
		if (payload_length !=
		    PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		memcpy(pending_value, response->pending_and_present_values, 1);
		memcpy(present_value, &response->pending_and_present_values[1],
		       1);

	} else if (*effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT16 ||
		   *effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT16) {
		if (payload_length !=
		    PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES + 2) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		memcpy(pending_value, response->pending_and_present_values, 2);
		memcpy(present_value, &response->pending_and_present_values[2],
		       2);

	} else if (*effecter_data_size == PLDM_EFFECTER_DATA_SIZE_UINT32 ||
		   *effecter_data_size == PLDM_EFFECTER_DATA_SIZE_SINT32) {
		if (payload_length !=
		    PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES + 6) {
			return PLDM_ERROR_INVALID_LENGTH;
		}
		memcpy(pending_value, response->pending_and_present_values, 4);
		memcpy(present_value, &response->pending_and_present_values[4],
		       4);
	}
	return PLDM_SUCCESS;
}
