#ifndef FILEIO_H
#define FILEIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "base.h"

/** @brief PLDM Commands in IBM OEM type
 */
enum pldm_fileio_commands {
	PLDM_GET_FILE_TABLE = 0x1,
	PLDM_READ_FILE_INTO_MEMORY = 0x6,
	PLDM_WRITE_FILE_FROM_MEMORY = 0x7,
};

/** @brief PLDM Command specific codes
 */
enum pldm_fileio_completion_codes {
	PLDM_INVALID_FILE_HANDLE = 0x80,
	PLDM_DATA_OUT_OF_RANGE = 0x81,
	PLDM_INVALID_READ_LENGTH = 0x82,
	PLDM_INVALID_WRITE_LENGTH = 0x83,
	PLDM_FILE_TABLE_UNAVAILABLE = 0x84,
	PLDM_INVALID_FILE_TABLE_TYPE = 0x85,
};

/** @brief PLDM File I/O table types
 */
enum pldm_fileio_table_type {
	PLDM_FILE_ATTRIBUTE_TABLE = 0,
	PLDM_OEM_FILE_ATTRIBUTE_TABLE = 1,
};

#define PLDM_RW_FILE_MEM_REQ_BYTES 20
#define PLDM_RW_FILE_MEM_RESP_BYTES 5
#define PLDM_GET_FILE_TABLE_REQ_BYTES 6
#define PLDM_GET_FILE_TABLE_MIN_RESP_BYTES 6

/** @brief Decode ReadFileIntoMemory and WriteFileFromMemory commands request
 *         data
 *
 *  @param[in] msg - Pointer to PLDM request message payload
 *  @param[in] payload_length - Length of request payload
 *  @param[out] file_handle - A handle to the file
 *  @param[out] offset - Offset to the file at which the read should begin
 *  @param[out] length - Number of bytes to be read
 *  @param[out] address - Memory address where the file content has to be
 *                        written to
 *  @return pldm_completion_codes
 */
int decode_rw_file_memory_req(const uint8_t *msg, size_t payload_length,
			      uint32_t *file_handle, uint32_t *offset,
			      uint32_t *length, uint64_t *address);

/** @brief Create a PLDM response for ReadFileIntoMemory and
 *         WriteFileFromMemory
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] command - PLDM command
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] length - Number of bytes read. This could be less than what the
			 requester asked for.
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'msg'
 */
int encode_rw_file_memory_resp(uint8_t instance_id, uint8_t command,
			       uint8_t completion_code, uint32_t length,
			       struct pldm_msg *msg);

/** @struct pldm_get_file_table_req
 *
 *  Structure representing GetFileTable request
 */
struct pldm_get_file_table_req {
	uint32_t transfer_handle; //!< Data transfer handle
	uint8_t operation_flag;   //!< Transfer operation flag
	uint8_t table_type;       //!< Table type
} __attribute__((packed));

/** @struct pldm_get_file_table_resp
 *
 *  Structure representing GetFileTable response fixed data
 */
struct pldm_get_file_table_resp {
	uint8_t completion_code;       //!< Completion code
	uint32_t next_transfer_handle; //!< Next data transfer handle
	uint8_t transfer_flag;	 //!< Transfer flag
	uint8_t table_data[1];	 //!< Table Data
} __attribute__((packed));

/** @brief Decode GetFileTable command request data
 *
 *  @param[in] msg - Pointer to PLDM request message payload
 *  @param[in] payload_length - Length of request payload
 *  @param[out] trasnfer_handle - the handle of data
 *  @param[out] transfer_opflag - Transfer operation flag
 *  @param[out] table_type - the type of file table
 *  @return pldm_completion_codes
 */
int decode_get_file_table_req(const uint8_t *msg, size_t payload_length,
			      uint32_t *transfer_handle,
			      uint8_t *transfer_opflag, uint8_t *table_type);

/** @brief Create a PLDM response for GetFileTable command
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] next_transfer_handle - Handle to identify next portion of
 *              data transfer
 *  @param[in] transfer_flag - Represents the part of transfer
 *  @param[in] table_data - pointer to file table data
 *  @param[in] table_size - file table size
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'msg'
 */
int encode_get_file_table_resp(uint8_t instance_id, uint8_t completion_code,
			       uint32_t next_transfer_handle,
			       uint8_t transfer_flag, const uint8_t *table_data,
			       size_t table_size, struct pldm_msg *msg);

#ifdef __cplusplus
}
#endif

#endif /* FILEIO_H */
