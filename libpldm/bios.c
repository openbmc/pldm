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

int decode_get_date_time_resp(const uint8_t *msg, size_t payload_length,
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
	    (struct pldm_get_date_time_resp *)msg;
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
