#ifndef FRU_H
#define FRU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <stddef.h>
#include <stdint.h>

#include "base.h"

#define PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES 5
#define PLDM_GET_FRU_RECORD_TABLE_RESP_BYTES 6

/** @brief PLDM FRU commands
 */
enum pldm_fru_commands {
	PLDM_GET_FRU_RECORD_TABLE_METADATA = 0X01,
	PLDM_GET_FRU_RECORD_TABLE = 0X02,
	PLDM_SET_FRU_RECORD_TABLE = 0X03,
	PLDM_GET_FRU_RECORD_BY_OPTION = 0X04,
};

/** @struct pldm_get_fru_record_table_req
 *
 *  Structure representing PLDM get FRU record table request.
 */
struct pldm_get_fru_record_table_req {
	uint32_t data_transfer_handle;
	uint8_t transfer_operation_flag;
} __attribute__((packed));

/** @struct pldm_get_fru_record_table_resp
 *
 *  Structure representing PLDM get FRU record table response.
 */
struct pldm_get_fru_record_table_resp {
	uint8_t completion_code;
	uint32_t next_data_transfer_handle;
	uint8_t transfer_flag;
	uint8_t fru_record[1];
} __attribute__((packed));

/* Requester */

/* GetFruRecordTable */

/* @brief Create a PLDM request message for GetFruRecordTable
 *
 * @param[in] instance_id - Message's instance id
 * @param[in] data_transfer_handle - A handle, used to identify a FRU Record
 * Table data transfer
 * @param[in] transfer_operation_flag - A flag that indicates whether this is
 * the start of the transfer
 * @param[in,out] msg - Message will be written to this
 * @return pldm_completion_codes
 * @note  Caller is responsible for memory alloc and dealloc of param
 *        'msg.body.payload'
 */

int encode_get_fru_record_table_req(uint8_t instance_id,
				    uint32_t data_transfer_handle,
				    uint8_t transfer_operation_flag,
				    struct pldm_msg *msg);

/* @brief Decode GetFruRecordTable response data
 *
 * @param[in] msg - Response message
 * @param[in] payload_length - Length of response message payload
 * @param[out] completion_code - Pointer to response msg's PLDM completion code
 * @param[out] next_data_transfer_handle - A handle used to identify the next
 * portion of the transfer
 * @param[out] transfer_flag - The transfer flag that indicates what part of the
 * transfer this response represents
 * @param[out] fru_record_offset - Offset indicating the portion of overall FRU
 * record table
 * @return pldm_completion_codes
 */

int decode_get_fru_record_table_resp(const struct pldm_msg *msg,
				     size_t payload_length,
				     uint8_t *completion_code,
				     uint32_t *next_data_transfer_handle,
				     uint8_t *transfer_flag,
				     size_t *fru_record_offset);

/* Responder */

/* GetFruRecordTable */

/* @brief Create a PLDM response message for GetFruRecordTable
 *
 * @param[in] instance_id - Message's instance id
 * @param[in] completion_code - PLDM completion code
 * @param[in] next_data_transfer_handle - A handle that is used to identify the
 * next portion of the transfer
 * @param[in] transfer_flag - The transfer flag that indicates what part of the
 * transfer this response represents
 * @param[in,out] msg - Message will be written to this
 * @return pldm_completion_codes
 * @note  Caller is responsible for memory alloc and dealloc of param 'msg'.
 */

int encode_get_fru_record_table_resp(uint8_t instance_id,
				     uint8_t completion_code,
				     uint32_t next_data_transfer_handle,
				     uint8_t transfer_flag,
				     struct pldm_msg *msg);

/* @brief Decode GetFruRecordTable request data
 *
 * @param[in] msg - PLDM request message payload
 * @param[in] payload_length - Length of request payload
 * @param[out] data_transfer_handle - A handle, used to identify a FRU Record
 * Table data transfer
 * @param[out] transfer_operation_flag - A flag that indicates whether this is
 * the start of the transfer
 * @return pldm_completion_codes
 */

int decode_get_fru_record_table_req(const struct pldm_msg *msg,
				    size_t payload_length,
				    uint32_t *data_transfer_handle,
				    uint8_t *transfer_operation_flag);

#ifdef __cplusplus
}
#endif

#endif
