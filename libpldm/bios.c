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

	msg->payload[0] = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_BIOS;
	header.command = PLDM_GET_DATE_TIME;
	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	uint8_t *dst = msg->payload + sizeof(msg->payload[0]);

	memcpy(dst, &seconds, sizeof(seconds));
	dst += sizeof(seconds);
	memcpy(dst, &minutes, sizeof(minutes));
	dst += sizeof(minutes);
	memcpy(dst, &hours, sizeof(hours));
	dst += sizeof(hours);
	memcpy(dst, &day, sizeof(day));
	dst += sizeof(day);
	memcpy(dst, &month, sizeof(month));
	dst += sizeof(month);
	uint16_t local_year = htole16(year);
	memcpy(dst, &local_year, sizeof(local_year));

	return PLDM_SUCCESS;
}

int decode_get_date_time_resp(const uint8_t *msg, uint8_t *completion_code,
			      uint8_t *seconds, uint8_t *minutes,
			      uint8_t *hours, uint8_t *day, uint8_t *month,
			      uint16_t *year)
{
	if (msg == NULL || seconds == NULL || minutes == NULL ||
	    hours == NULL || day == NULL || month == NULL || year == NULL ||
	    completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg[0];
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	}
	const uint8_t *start = msg + sizeof(uint8_t);
	*seconds = *start;
	*minutes = *(start + sizeof(*seconds));
	*hours = *(start + sizeof(*seconds) + sizeof(*minutes));
	*day = *(start + sizeof(*seconds) + sizeof(*minutes) + sizeof(*hours));
	*month = *(start + sizeof(*seconds) + sizeof(*minutes) +
		   sizeof(*hours) + sizeof(*day));
	*year = le16toh(
	    *((uint16_t *)(start + sizeof(*seconds) + sizeof(*minutes) +
			   sizeof(*hours) + sizeof(*day) + sizeof(*month))));

	return PLDM_SUCCESS;
}
