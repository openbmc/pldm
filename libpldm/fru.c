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

