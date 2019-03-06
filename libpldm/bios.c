#include <endian.h>
#include <string.h>

#include "bios.h"

int encode_get_dateTime_req(uint8_t instance_id, struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	header.instance = instance_id;
	header.msg_type = PLDM_REQUEST;
	header.command = PLDM_GET_DATE_TIME;
	header.pldm_type = PLDM_BIOS;
	rc = pack_pldm_header(&header, &(msg->hdr));

	return rc;
}

int encode_get_dateTime_resp(uint8_t instance_id, uint8_t completion_code,
			     const uint8_t *seconds, const uint8_t *minutes,
			     const uint8_t *hours, const uint8_t *day,
			     const uint8_t *month, const uint16_t *year,
			     struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	msg->body.payload[0] = completion_code;

	header.instance = instance_id;
	header.msg_type = PLDM_RESPONSE;
	header.command = PLDM_GET_DATE_TIME;
	header.pldm_type = PLDM_BIOS;
	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	uint8_t *dst = msg->body.payload + sizeof(msg->body.payload[0]);

	memcpy(dst, seconds, sizeof(*seconds));
	dst += sizeof(*seconds);
	memcpy(dst, minutes, sizeof(*minutes));
	dst += sizeof(*minutes);
	memcpy(dst, hours, sizeof(*hours));
	dst += sizeof(*hours);
	memcpy(dst, day, sizeof(*day));
	dst += sizeof(*day);
	memcpy(dst, month, sizeof(*month));
	dst += sizeof(*month);
	uint16_t local_year = htole16(*year);
	memcpy(dst, &local_year, sizeof(local_year));

	return PLDM_SUCCESS;
}

int decode_get_dateTime_resp(const struct pldm_msg_payload *msg,
			     uint8_t *seconds, uint8_t *minutes, uint8_t *hours,
			     uint8_t *day, uint8_t *month, uint16_t *year)
{
	if (msg == NULL || seconds == NULL || minutes == NULL ||
	    hours == NULL || day == NULL || month == NULL || year == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	const uint8_t *start = msg->payload + sizeof(uint8_t);
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
