#ifndef FRU_H
#define FRU_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stddef.h>
#include <stdint.h>
#include <asm/byteorder.h>

#include "base.h"

#define PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES 5
#define PLDM_GET_FRU_RECORD_TABLE_RESP_BYTES 11

enum pldm_fru_commands{
 PLDM_GET_FRU_RECORD_TABLE_METADATA = 0X01,
 PLDM_GET_FRU_rECORD_TABLE = 0X02,
 PLDM_SET_FRU_RECORD_TABLE = 0X03,
 PLDM_GET_FRU_RECORD_BY_OPTION 0X04,
};

struct pldm_get_fru_record_table_req{
	uint32_t data_transfer_handle;
	uint8_t transfer_operation_flag;
}__attribute__((packed));

struct pldm_get_fru_record_table_resp{
	uint8_t completion_code;
	uint32_t next_data_transfer_handle;
	uint8_t transfer_flag;
	uint8_t fru_record[1];
}__attribute__((packed));

int encode_get_fru_record_table_req(uint8_t instance_id, uint32_t data_transfer_handle, uint8_t transfer_operation_flag, struct pldm_msg *msg);

int decode_get_fru_record_table_resp(const struct pldm_msg *msg, size_t payload_length, uint8_t completion_code, uint32_t next_data_tansfer_handle, uint8_t transfer_flag, size_t *fru_record_offset );

int encode_get_fru_record_table_resp(uint8_t instance_id, uint8_t completion_code, uint32_t next_data_tansfer_handle, uint8_t transfer_flag, struct pldm_msg *msg);

int decode_get_fru_record_table_req(const struct pldm_msg *msg, size_t payload_length, uint32_t data_transfer_handle, uint8_t transfer_operation_flag);




