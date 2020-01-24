#ifndef FRU_H
#define FRU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <stddef.h>
#include <stdint.h>

#include "base.h"

#define PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES 19
#define PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES 5
#define PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES 6

/** @brief PLDM FRU commands
 */
enum pldm_fru_commands {
	PLDM_GET_FRU_RECORD_TABLE_METADATA = 0X01,
	PLDM_GET_FRU_RECORD_TABLE = 0X02,
};

/** @brief FRU record types
 */
enum pldm_fru_record_type {
	PLDM_FRU_RECORD_TYPE_GENERAL = 0X01,
	PLDM_FRU_RECORD_TYPE_OEM = 0XFE,
};

/** @brief Encoding type for FRU fields
 */
enum pldm_fru_field_encoding {
	PLDM_FRU_ENCODING_UNSPECIFIED = 0X00,
	PLDM_FRU_ENCODING_ASCII = 0X01,
	PLDM_FRU_ENCODING_UTF8 = 0X02,
	PLDM_FRU_ENCODING_UTF16 = 0X03,
	PLDM_FRU_ENCODING_UTF16LE = 0X04,
	PLDM_FRU_ENCODING_UTF16BE = 0X05,
};

/** @brief FRU field types
 */
enum pldm_fru_field_type {
	PLDM_FRU_FIELD_TYPE_CHASSIS = 0X01,
	PLDM_FRU_FIELD_TYPE_MODEL = 0X02,
	PLDM_FRU_FIELD_TYPE_PN = 0X03,
	PLDM_FRU_FIELD_TYPE_SN = 0X04,
	PLDM_FRU_FIELD_TYPE_MANUFAC = 0X05,
	PLDM_FRU_FIELD_TYPE_MANUFAC_DATE = 0X06,
	PLDM_FRU_FIELD_TYPE_VENDOR = 0X07,
	PLDM_FRU_FIELD_TYPE_NAME = 0X08,
	PLDM_FRU_FIELD_TYPE_SKU = 0X09,
	PLDM_FRU_FIELD_TYPE_VERSION = 0X0A,
	PLDM_FRU_FIELD_TYPE_ASSET_TAG = 0X0B,
	PLDM_FRU_FIELD_TYPE_DESC = 0X0C,
	PLDM_FRU_FIELD_TYPE_EC_LVL = 0X0D,
	PLDM_FRU_FIELD_TYPE_OTHER = 0X0E,
	PLDM_FRU_FIELD_TYPE_IANA = 0X0F,
};

/** @struct pldm_get_fru_record_table_metadata_resp
 *
 *  Structure representing PLDM get FRU table metadata response.
 */
struct pldm_get_fru_record_table_metadata_resp {
	uint8_t completion_code;	//!< completion code
	uint8_t fru_data_major_version; //!< The major version of the FRU Record
	uint8_t fru_data_minor_version; //!< The minor version of the FRU Record
	uint32_t
	    fru_table_maximum_size; //!< The size of the largest FRU Record data
	uint32_t fru_table_length; //!< The total length of the FRU Record Table
	uint16_t total_record_set_identifiers; //!< The total number of FRU
					       //!< Record Data structures
	uint16_t
	    total_table_records; //!< The total number of records in the table
	uint32_t
	    checksum; //!< The integrity checksum on the FRU Record Table data
} __attribute__((packed));

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

/** @struct pldm_fru_record_tlv
 *
 *  Structure representing each FRU field entry (type, length, value)
 */
struct pldm_fru_record_tlv {
	uint8_t type;
	uint8_t length;
	uint8_t value[1];
} __attribute__((packed));

/** @struct pldm_fru_record_data_format
 *
 *  Structure representing the FRU record data format
 */
struct pldm_fru_record_data_format {
	uint16_t record_set_id;
	uint8_t record_type;
	uint8_t num_fru_fields;
	uint8_t encoding_type;
	struct pldm_fru_record_tlv tlvs[1];
} __attribute__((packed));

/* Requester */

/* GetFRURecordTableMetadata */

/** @brief Create a PLDM request message for GetFRURecordTableMetadata
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_get_fru_record_table_metadata_req(uint8_t instance_id,
					     struct pldm_msg *msg);

/** @brief Decode GetFruRecordTable response data
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] fru_data_major_version - Major version of the FRU Record
 *  @param[out] fru_data_minor_version - Minor version of the FRU Record
 *  @param[out] fru_table_maximum_size - Size of the largest FRU Record data
 *  @param[out] fru_table_length - Total length of the FRU Record Table
 *  @param[out] total_Record_Set_Identifiers - Total number of FRU Record Data
 * structures
 *  @param[out] total_table_records - Total number of records in the table
 *  @param[out] checksum - integrity checksum on the FRU Record Table data
 *  @return pldm_completion_codes
 */
int decode_get_fru_record_table_metadata_resp(
    const struct pldm_msg *msg, size_t payload_length, uint8_t *completion_code,
    uint8_t *fru_data_major_version, uint8_t *fru_data_minor_version,
    uint32_t *fru_table_maximum_size, uint32_t *fru_table_length,
    uint16_t *total_record_set_identifiers, uint16_t *total_table_records,
    uint32_t *checksum);

/* Responder */

/* GetFRURecordTableMetadata */

/** @brief Create a PLDM response message for GetFRURecordTableMetadata
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] fru_data_major_version - Major version of the FRU Record
 *  @param[in] fru_data_minor_version - Minor version of the FRU Record
 *  @param[in] fru_table_maximum_size - Size of the largest FRU Record data
 *  @param[in] fru_table_length - Total length of the FRU Record Table
 *  @param[in] total_Record_Set_Identifiers - Total number of FRU Record Data
 * structures
 *  @param[in] total_table_records - Total number of records in the table
 *  @param[in] checksum - integrity checksum on the FRU Record Table data
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */

int encode_get_fru_record_table_metadata_resp(
    uint8_t instance_id, uint8_t completion_code,
    uint8_t fru_data_major_version, uint8_t fru_data_minor_version,
    uint32_t fru_table_maximum_size, uint32_t fru_table_length,
    uint16_t total_record_set_identifiers, uint16_t total_table_records,
    uint32_t checksum, struct pldm_msg *msg);

/* GetFruRecordTable */

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

/** @brief Create a PLDM response message for GetFruRecordTable
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] next_data_transfer_handle - A handle that is used to identify the
 *  next portion of the transfer
 *  @param[in] transfer_flag - The transfer flag that indicates what part of the
 *  transfer this response represents
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'msg',
 *         and for appending the FRU table to the msg.
 */
int encode_get_fru_record_table_resp(uint8_t instance_id,
				     uint8_t completion_code,
				     uint32_t next_data_transfer_handle,
				     uint8_t transfer_flag,
				     struct pldm_msg *msg);

/** @brief Encode the FRU record in the FRU table
 *
 *  @param[in/out] fru_table - Pointer to the FRU table
 *  @param[in] total_size - The size of the table,including the size of FRU
 *                          record to be added to the table.
 *  @param[in/out] curr_size - The size of the table, excluding the size of FRU
 *                          record to be added to the table.
 *  @param[in] record_set_id - FRU record set identifier
 *  @param[in] record_type - FRU record type
 *  @param[in] num_frus - Number of FRU fields
 *  @param[in] encoding - Encoding type for FRU fields
 *  @param[in] tlvs - Pointer to the buffer with all the FRU fields
 *  @param[in] tlvs_size - Size of the  buffer with all the FRU fields
 *
 *  @return pldm_completion_codes
 */
int encode_fru_record(uint8_t *fru_table, size_t total_size, size_t *curr_size,
		      uint16_t record_set_id, uint8_t record_type,
		      uint8_t num_frus, uint8_t encoding, uint8_t *tlvs,
		      size_t tlvs_size);

#ifdef __cplusplus
}
#endif

#endif
