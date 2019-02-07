#ifndef BASE_H
#define BASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <stddef.h>
#include <stdint.h>

/** @brief API return codes
 */
enum error_codes { OK = 0, FAIL = -1 };

/** @brief PLDM base codes
 */
enum pldm_completion_codes {
	SUCCESS = 0x00,
	ERROR = 0x01,
	ERROR_INVALID_DATA = 0x02,
	ERROR_INVALID_LENGTH = 0x03,
	ERROR_NOT_READY = 0x04,
	ERROR_UNSUPPORTED_PLDM_CMD = 0x05,
	ERROR_INVALID_PLDM_TYPE = 0x20
};

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

#define PLDM_REQUEST_HEADER_SIZE sizeof(struct pldm_header_req)
#define PLDM_RESPONSE_HEADER_SIZE sizeof(struct pldm_header_resp)
#define PLDM_INSTANCE_MAX 31
#define PLDM_TYPE_MAX 63

typedef uint32_t ver32;

#define PLDM_REQUEST_HEADER_LEN_BYTES 3
#define PLDM_RESPONSE_HEADER_LEN_BYTES 4

#define PLDM_GET_TYPES_REQ_DATA_BYTES 0
#define PLDM_GET_TYPES_RESP_DATA_BYTES 8
#define PLDM_MAX_TYPES 64

#define PLDM_GET_COMMANDS_REQ_DATA_BYTES 5
#define PLDM_GET_COMMANDS_RESP_DATA_BYTES 32
#define PLDM_MAX_CMDS_PER_TYPE 256

/**
 * @brief Populate the PLDM message with the PLDM header.The caller of this API
 *        allocates space for the PLDM header when forming the PLDM message. The
 *        PLDM header size is different for request and response.
 *
 * @param[out] ptr - Pointer to the PLDM message
 * @param[in] hdr - Pointer to the PLDM header information
 *
 * @return 0 on success, negative error code on failure
 */
int pack_pldm_header(uint8_t *ptr, struct pldm_header_info *hdr);

/**
 * @brief Unpack the PLDM header and the PLDM message payload from the PLDM
 *        message. This function does not do memory allocation.
 *
 * @param[in] ptr - Pointer to the PLDM message
 * @param[in] size - Size of the PLDM message
 * @param[out] hdr - Pointer to the PLDM header information
 * @param[out] offset - Offset to the PLDM message payload
 * @param[out] payload_size - Size of the PLDM message payload size
 *
 * @return 0 on success, negative error code on failure
 */
int unpack_pldm_header(uint8_t *ptr, size_t size, struct pldm_header_info *hdr,
		       size_t *offset, size_t *payload_size);

/* Requester */

/* GetPLDMTypes */

/** @brief Create a PLDM request message for GetPLDMTypes
 *
 *  @param instance_id[in] Message's instance id
 *  @param buffer[in/out] Message will be written to this buffer.
 *         Caller is responsible for memory alloc and dealloc.
 *  @return 0 on success, negative error code on failure
 */
int encode_get_types_req(uint8_t instance_id, uint8_t *buffer);

/* GetPLDMCommands */

/** @brief Create a PLDM request message for GetPLDMCommands
 *
 *  @param instance_id[in] Message's instance id
 *  @param type[in] PLDM Type
 *  @param version[in] Version for PLDM Type
 *  @param buffer[in/out] Message will be written to this buffer.
 *         Caller is responsible for memory alloc and dealloc.
 *  @return 0 on success, negative error code on failure
 */
int encode_get_commands_req(uint8_t instance_id, uint8_t type, ver32 version,
			    uint8_t *buffer);

/* Responder */

/* GetPLDMTypes */

/** @brief Create a PLDM response message for GetPLDMTypes
 *
 *  @param instance_id[in] Message's instance id
 *  @param completion_cpde[in] PLDM completion code
 *  @param types[in] pointer to array uint8_t[8] containing supported
 *         types (MAX_TYPES/8) = 8), as per DSP0240.
 *  @param buffer[in/out] Message will be written to this buffer.
 *         Caller is responsible for memory alloc and dealloc.
 *  @return 0 on success, negative error code on failure
 */
int encode_get_types_resp(uint8_t instance_id, uint8_t completion_code,
			  const uint8_t *types, uint8_t *buffer);

/* GetPLDMCommands */

/** @brief Decode GetPLDMCommands' request data
 *
 *  @param payload[in] Request data
 *  @param type[out] PLDM Type
 *  @param version[out] Version for PLDM Type
 *  @return 0 on success, negative error code on failure
 */
int decode_get_commands_req(const uint8_t *request, uint8_t *type,
			    ver32 *version);

/** @brief Create a PLDM response message for GetPLDMCommands
 *
 *  @param instance_id[in] Message's instance id
 *  @param completion_cpde[in] PLDM completion code
 *  @param types[in] pointer to array uint8_t[32] containing supported
 *         types (MAX_CMDS_PER_TYPE/8) = 32), as per DSP0240.
 *  @param buffer[in/out] Message will be written to this buffer.
 *         Caller is responsible for memory alloc and dealloc.
 *  @return 0 on success, negative error code on failure
 */
int encode_get_commands_resp(uint8_t instance_id, uint8_t completion_code,
			     const uint8_t *commands, uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif /* BASE_H */
