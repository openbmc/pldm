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

	struct PLDM_GetDateTime_Response *dst =
	    (struct PLDM_GetDateTime_Response *)msg->body.payload;
	header.instance = instance_id;
	header.pldm_type = PLDM_BIOS;
	header.command = PLDM_GET_DATE_TIME;
	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	dst->completion_code = completion_code;
	dst->seconds = seconds;
	dst->minutes = minutes;
	dst->hours = hours;
	dst->day = day;
	dst->month = month;
	dst->year = htole16(year);
	return PLDM_SUCCESS;
}

int decode_get_date_time_resp(const struct pldm_msg_payload *msg,
			      uint8_t *completion_code, uint8_t *seconds,
			      uint8_t *minutes, uint8_t *hours, uint8_t *day,
			      uint8_t *month, uint16_t *year)
{
	if (msg == NULL || seconds == NULL || minutes == NULL ||
	    hours == NULL || day == NULL || month == NULL || year == NULL ||
	    completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	struct PLDM_GetDateTime_Response *start =
	    (struct PLDM_GetDateTime_Response *)msg->payload;
	*completion_code = start->completion_code;

	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	}
	*seconds = start->seconds;
	*minutes = start->minutes;
	*hours = start->hours;
	*day = start->day;
	*month = start->month;
	*year = le16toh(start->year);

	return PLDM_SUCCESS;
}
