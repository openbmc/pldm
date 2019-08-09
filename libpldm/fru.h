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
#define PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES 6
#define PLDM_GET_FRU_RECORD_BY_OPTION_REQ_BYTES 11
#define PLDM_GET_FRU_RECORD_BY_OPTION_MIN_RESP_BYTES 6

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
	uint8_t fru_record_table_data[1];
} __attribute__((packed));

/** @struct pldm_get_fru_record_by_option_req
 *
 *  Structure representing PLDM get FRU record by option request.
 */
struct pldm_get_fru_record_by_option_req {
	uint32_t data_transfer_handle;
	uint16_t fru_table_handle;
	uint16_t record_set_identifier;
	uint8_t record_type;
	uint8_t field_type;
	uint8_t transfer_operation_flag;
} __attribute__((packed));

/** @struct pldm_get_fru_record_by_option_resp
 *
 *  Structure representing PLDM get FRU record by option response.
 */
struct pldm_get_fru_record_by_option_resp {
	uint8_t completion_code;
	uint32_t next_data_transfer_handle;
	uint8_t transfer_flag;
	uint8_t fru_data_structure_data[1];
} __attribute__((packed));

/* Requester */

/* GetFruRecordTable */

/** @brief Create a PLDM request message for GetFruRecordTable
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] data_transfer_handle - A handle, used to identify a FRU Record
 *  Table data transfer
 *  @param[in] transfer_operation_flag - A flag that indicates whether this is
 *  the start of the transfer
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */

int encode_get_fru_record_table_req(uint8_t instance_id,
				    uint32_t data_transfer_handle,
				    uint8_t transfer_operation_flag,
				    struct pldm_msg *msg);

/** @brief Decode GetFruRecordTable response data
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] next_data_transfer_handle - A handle used to identify the next
 *  portion of the transfer
 *  @param[out] transfer_flag - The transfer flag that indicates what part of
 * the transfer this response represents
 *  @param[out] fru_record_table_data - This data is a portion of the overall
 * FRU Record Table
 *  @param[out] fru_record_table_length - Length of the FRU record table data
 *  @return pldm_completion_codes
 */

int decode_get_fru_record_table_resp(
    const struct pldm_msg *msg, size_t payload_length, uint8_t *completion_code,
    uint32_t *next_data_transfer_handle, uint8_t *transfer_flag,
    uint8_t *fru_record_table_data, size_t *fru_record_table_length);

/* GetFruRecordByOption */

/** @brief Create a PLDM request message for GetFruRecordByOption
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] data_transfer_handle - A handle that is used to identify FRU
 *  Record Data transfer
 *  @param[in] fru_table_handle - A handle that is used to identify FRU DATA
 *  records
 *  @param[in] record_set_identifier - Identifier for each record set
 *  @param[in] record_type - Specifies the record type
 *  @param[in] field_type - Specifies record field type
 *  @param[in] transfer_operation_flag - The operation flag that indicates
 *  whether this is the start of the transfer
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'msg'.
 */

int encode_get_fru_record_by_option_req(uint8_t instance_id,
					uint32_t data_transfer_handle,
					uint16_t fru_table_handle,
					uint16_t record_set_identifier,
					uint8_t record_type, uint8_t field_type,
					uint8_t transfer_operation_flag,
					struct pldm_msg *msg);

/** @brief Decode GetFruRecordByOption response data
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] next_data_transfer_handle - A handle that is used to identify
 * the next portion of the transfer
 *  @param[out] transfer_flag - The transfer flag that indicates what part of
 * the transfer this response represents
 *  @param[out] fru_data_structure_data_offset - Offset representing the fru
 *  record data
 *  @return pldm_completion_codes
 */

int decode_get_fru_record_by_option_resp(
    const struct pldm_msg *msg, size_t payload_length, uint8_t *completion_code,
    uint32_t *next_data_transfer_handle, uint8_t *transfer_flag,
    uint8_t *fru_data_structure_data, size_t *fru_data_structure_data_length);

/* Responder */

/* GetFruRecordTable */

/** @brief Create a PLDM response message for GetFruRecordTable
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] next_data_transfer_handle - A handle that is used to identify the
 *  next portion of the transfer
 *  @param[in] transfer_flag - The transfer flag that indicates what part of the
 *  transfer this response represents
 *  @param[in] fru_record_table_data - This data is a portion of the overall
 * FRU Record Table
 *  @param[in] payload_length - Length of the payload message
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'msg'.
 */

int encode_get_fru_record_table_resp(uint8_t instance_id,
				     uint8_t completion_code,
				     uint32_t next_data_transfer_handle,
				     uint8_t transfer_flag,
				     uint8_t *fru_record_table_data,
				     size_t payload_length,
				     struct pldm_msg *msg);

/** @brief Decode GetFruRecordTable request data
 *
 *  @param[in] msg - PLDM request message payload
 *  @param[in] payload_length - Length of request payload
 *  @param[out] data_transfer_handle - A handle, used to identify a FRU Record
 *  Table data transfer
 *  @param[out] transfer_operation_flag - A flag that indicates whether this is
 *  the start of the transfer
 *  @return pldm_completion_codes
 */

int decode_get_fru_record_table_req(const struct pldm_msg *msg,
				    size_t payload_length,
				    uint32_t *data_transfer_handle,
				    uint8_t *transfer_operation_flag);

/* GetFruRecordByOption */

/** @brief Create a PLDM response message for GetFruRecordByOption
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] next_data_transfer_handle - A handle that is used to identify the
 *  next portion of the transfer
 *  @param[in] transfer_flag - The transfer flag that indicates what part of the
 *  transfer this response represents
 *  @param[in] fru_data_structure_data - This data is a portion of the overall
 * FRU Record data
 *  @param[in] payload_length - Length of the payload message
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'msg'.
 */

int encode_get_fru_record_by_option_resp(uint8_t instance_id,
					 uint8_t completion_code,
					 uint32_t next_data_transfer_handle,
					 uint8_t transfer_flag,
					 uint8_t *fru_data_structure_data,
					 size_t payload_length,
					 struct pldm_msg *msg);

#ifdef __cplusplus
}
#endif

#endif
