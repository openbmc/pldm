#ifndef BASE_H
#define BASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <stddef.h>
#include <stdint.h>

/** @brief PLDM Types
 */
enum pldm_supported_types {
	PLDM_BASE = 0x00,
};

/** @brief PLDM Commands
 */
enum pldm_supported_commands {
	PLDM_GET_PLDM_TYPES = 0x4,
	PLDM_GET_PLDM_COMMANDS = 0x5
};

/** @brief PLDM base codes
 */
enum pldm_completion_codes {
	PLDM_SUCCESS = 0x00,
	PLDM_ERROR = 0x01,
	PLDM_ERROR_INVALID_DATA = 0x02,
	PLDM_ERROR_INVALID_LENGTH = 0x03,
	PLDM_ERROR_NOT_READY = 0x04,
	PLDM_ERROR_UNSUPPORTED_PLDM_CMD = 0x05,
	PLDM_ERROR_INVALID_PLDM_TYPE = 0x20
};

enum TRANSFER_OP_FLAG { GetNextPart = 0, GetFirstPart };

enum TRANSFER_RESP_FLAG { START = 1, MIDDLE = 2, END, START_AND_END };

/** @enum MessageType
 *
 *  The different message types supported by the PLDM specification.
 */
typedef enum {
	RESPONSE,	     //!< PLDM response
	REQUEST,	      //!< PLDM request
	RESERVED,	     //!< Reserved
	ASYNC_REQUEST_NOTIFY, //!< Unacknowledged PLDM request messages
} MessageType;

/** @struct pldm_header_req
 *
 *  Format of the header in a PLDM request message.
 */
struct pldm_header_req {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t instance_id : 5; //!< Instance ID
	uint8_t reserved : 1;    //!< Reserved
	uint8_t datagram : 1;    //!< Datagram bit
	uint8_t request : 1;     //!< Request bit
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint8_t request : 1;     //!< Request bit
	uint8_t datagram : 1;    //!< Datagram bit
	uint8_t reserved : 1;    //!< Reserved
	uint8_t instance_id : 5; //!< Instance ID
#endif

#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t type : 6;       //!< PLDM type
	uint8_t header_ver : 2; //!< Header version
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint8_t header_ver : 2;  //!< Header version
	uint8_t type : 6;	//!< PLDM type
#endif
	uint8_t command; //!< PLDM command code
} __attribute__((packed));

/** @struct pldm_header_resp
 *
 *  Format of the header in a PLDM response message.
 */
struct pldm_header_resp {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t instance_id : 5; //!< Instance ID
	uint8_t reserved : 1;    //!< Reserved
	uint8_t datagram : 1;    //!< Datagram bit
	uint8_t request : 1;     //!< Request bit
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint8_t request : 1;     //!< Request bit
	uint8_t datagram : 1;    //!< Datagram bit
	uint8_t reserved : 1;    //!< Reserved
	uint8_t instance_id : 5; //!< Instance ID
#endif

#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t type : 6;       //!< PLDM type
	uint8_t header_ver : 2; //!< Header version
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint8_t header_ver : 2;  //!< Header version
	uint8_t type : 6;	//!< PLDM type
#endif
	uint8_t command;	 //!< PLDM command code
	uint8_t completion_code; //!< PLDM completion code
} __attribute__((packed));

/** @struct pldm_header_info
 *
 *  The information needed to prepare PLDM header and this is passed to the
 *  pack_pldm_header and unpack_pldm_header API.
 */
struct pldm_header_info {
	MessageType msg_type;    /* PLDM message type*/
	uint8_t instance;	/* PLDM instance id */
	uint8_t pldm_type;       /* PLDM type */
	uint8_t command;	 /* PLDM command code */
	uint8_t completion_code; /* PLDM completion code, applies only for
			response */
};

#define PLDM_REQUEST_HEADER_LEN_BYTES sizeof(struct pldm_header_req)
#define PLDM_RESPONSE_HEADER_LEN_BYTES sizeof(struct pldm_header_resp)
#define PLDM_MAX_TYPES 64
#define PLDM_INSTANCE_MAX 31

#define PLDM_GET_TYPES_REQ_DATA_BYTES 0
#define PLDM_GET_TYPES_RESP_DATA_BYTES 8

#define PLDM_GET_COMMANDS_REQ_DATA_BYTES 5
#define PLDM_GET_COMMANDS_RESP_DATA_BYTES 32
#define PLDM_MAX_CMDS_PER_TYPE 256

#define PLDM_GET_VERSION_REQ_DATA_BYTES 6
#define PLDM_GET_VERSION_RESP_DATA_BYTES 9

/** @struct pldm_version_t
 *
 * Structure representing PLDM ver32 type
 */
struct pldm_version_t {
	uint8_t major;
	uint8_t minor;
	uint8_t update;
	uint8_t alpha;
} __attribute__((packed));

/**
 * @brief Populate the PLDM message with the PLDM header.The
 *        PLDM header size is PLDM_REQUEST_HEADER_LEN_BYTES for request and
 *        PLDM_RESPONSE_HEADER_LEN_BYTES response. The caller of this API
 *        allocates buffer for the PLDM header when forming the PLDM message.
 *        The buffer is passed to this API to pack the PLDM header.
 *
 * @param[in] hdr - Pointer to the PLDM header information
 * @param[in,out] ptr - Pointer to the PLDM message
 *
 * @return 0 on success, otherwise PLDM error codes.
 */
int pack_pldm_header(const struct pldm_header_info *hdr, uint8_t *ptr);

/**
 * @brief Unpack the PLDM header and the PLDM message payload from the PLDM
 *        message. The pointer to the PLDM message and the size is passed as
 *        parameters. The header is unpacked, offset to the message payload and
 *        the size is returned.
 *
 * @param[in] ptr - Pointer to the PLDM message
 * @param[in] size - Size of the PLDM message
 * @param[out] hdr - Pointer to the PLDM header information
 * @param[out] offset - Offset to the PLDM message payload
 * @param[out] payload_size - Size of the PLDM message payload size
 *
 * @return 0 on success, otherwise PLDM error codes.
 */
int unpack_pldm_header(const uint8_t *ptr, const size_t size,
		       struct pldm_header_info *hdr, size_t *offset,
		       size_t *payload_size);

/* Requester */

/* GetPLDMTypes */

/** @brief Create a PLDM request message for GetPLDMTypes
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] buffer - Message will be written to this buffer
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'buffer'
 */
int encode_get_types_req(uint8_t instance_id, uint8_t *buffer);

/* GetPLDMCommands */

/** @brief Create a PLDM request message for GetPLDMCommands
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] type - PLDM Type
 *  @param[in] version - Version for PLDM Type
 *  @param[in,out] buffer - Message will be written to this buffer
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'buffer'
 */
int encode_get_commands_req(uint8_t instance_id, uint8_t type,
			    struct pldm_version_t version, uint8_t *buffer);

/* Responder */

/* GetPLDMTypes */

/** @brief Create a PLDM response message for GetPLDMTypes
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] types - pointer to array uint8_t[8] containing supported
 *             types (MAX_TYPES/8) = 8), as per DSP0240
 *  @param[in,out] buffer - Message will be written to this buffer
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'buffer'
 */
int encode_get_types_resp(uint8_t instance_id, uint8_t completion_code,
			  const uint8_t *types, uint8_t *buffer);

/* GetPLDMCommands */

/** @brief Decode GetPLDMCommands' request data
 *
 *  @param[in] request - Request data
 *  @param[out] type - PLDM Type
 *  @param[out] version - Version for PLDM Type
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'request'
 */
int decode_get_commands_req(const uint8_t *request, uint8_t *type,
			    struct pldm_version_t *version);

/** @brief Create a PLDM response message for GetPLDMCommands
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] commands - pointer to array uint8_t[32] containing supported
 *             commands (PLDM_MAX_CMDS_PER_TYPE/8) = 32), as per DSP0240
 *  @param[in,out] buffer - Message will be written to this buffer
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'buffer'
 */
int encode_get_commands_resp(uint8_t instance_id, uint8_t completion_code,
			     const uint8_t *commands, uint8_t *buffer);

/* GetPldmVersion  */

/** @brief Create a PLDM request for GetPLDMVersion
 *
 *  @param instance_id[in] Message's instance id
 *  @param transfer_handle[in] Handle to identify data transfer
 *  @param op_flag[in] flag to indicate whether it is start of transfer
 *  @type  Type for which version is requested
 *  @param buffer[in/out] Message will be written to this buffer
 *         Caller is responsible for memory alloc and dealloc
 *  @return 0 on success, negative error code on failure
 *  **/
int encode_get_version_req(uint8_t instance_id, uint32_t transfer_handle,
			   uint8_t op_flag, uint8_t type, uint8_t *buffer);

/** @brief Create a PLDM response for GetPLDMVersion
 *
 *  @param instance_id[in] Message's instance id
 *  @param completion_cpde[in] PLDM completion code
 *  @param next_transfer_handle[in] Handle to identify next portion of
 *              data transfer
 *  @param resp_flag[in] Represents the part of transfer
 *  @param version_data[in] the version data
 *  @param version_size[in] size of version data
 *  @param buffer[in/out] Message will be written to this buffer
 *  @param bufsize[in/out] Size of buffer
 *  **/
int encode_get_version_resp(uint8_t instance_id, uint8_t completion_code,
			    uint32_t next_transfer_handle, uint8_t resp_flag,
			    uint32_t *version_data, uint32_t version_size,
			    uint8_t *buffer, uint32_t bufsize);

/** @brief Decode a GetPLDMVersion request message
 *
 *  @param request[in] Request data
 *  @param transfer_handle[out] the handle of data
 *  @param op_flag[out] Transfer Flag
 *  @param type[out] PLDM type for which version is requested
 *  @return 0 on success, negative error code on failure
 *  **/
int decode_get_version_req(uint8_t *request, uint32_t *transfer_handle,
			   uint8_t *op_flag, uint8_t *type);

/** @brief Decode a GetPLDMVersion response message
 *
 *  @param buffer[in] Response payload
 *  @param bufsize[in] Total payload size
 *  @param offset[out] From where to read the version data
 *  @param completion_code[out] the response code
 *  @param next_transfer_handle[out] the next handle for the next part of data
 *  @param resp_flag[out] flag to indicate the part of data
 *  @return 0 on success, negative error code on failure
 *  **/
int decode_get_version_resp(uint8_t *buffer, uint32_t bufsize, uint32_t *offset,
			    uint8_t completion_code,
			    uint32_t *next_transfer_handle, uint8_t *resp_flag);

#ifdef __cplusplus
}
#endif

#endif /* BASE_H */
