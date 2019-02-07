#ifndef BASE_H
#define BASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <stddef.h>
#include <stdint.h>

struct pldm_header_req {
#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8_t instance_id : 5;
  uint8_t reserved : 1;
  uint8_t datagram : 1;
  uint8_t request : 1;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8_t request : 1;
  uint8_t datagram : 1;
  uint8_t reserved : 1;
  uint8_t instance_id : 5;
#endif

#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8_t type : 6;
  uint8_t header_ver : 2;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8_t header_ver : 2;
  uint8_t type : 6;
#endif
  uint8_t command;
} __attribute__((packed));

struct pldm_header_resp {
#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8_t instance_id : 5;
  uint8_t reserved : 1;
  uint8_t datagram : 1;
  uint8_t request : 1;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8_t request : 1;
  uint8_t datagram : 1;
  uint8_t reserved : 1;
  uint8_t instance_id : 5;
#endif

#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8_t type : 6;
  uint8_t header_ver : 2;
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8_t header_ver : 2;
  uint8_t type : 6;
#endif
  uint8_t command;
  uint8_t completion_code;
} __attribute__((packed));

#define PLDM_REQUEST_HEADER_SIZE sizeof(struct pldm_header_req)
#define PLDM_RESPONSE_HEADER_SIZE sizeof(struct pldm_header_resp)
#define PLDM_INSTANCE_MAX 31
#define PLDM_TYPE_MAX 63

/** @enum MessageType
 *
 *  The different message types supported by the PLDM specification.
 */
typedef enum {
  RESPONSE,                 /**< PLDM response */
  REQUEST,                  /**< PLDM request */
  ASYNC_REQUEST_NOTIFY,     /**< Unacknowledged PLDM request messages */
} MessageType;

struct pldm_header_info {
  MessageType msg_type;    /* PLDM message type*/
  uint8_t instance;        /* PLDM instance id */
  uint8_t pldm_type;       /* PLDM type */
  uint8_t command;         /* PLDM command code */
  uint8_t completion_code; /* PLDM completion code, applies only for response */
};

/**
 * @brief Populate the PLDM message by adding the PLDM header information to the
 *        message payload.
 *
 * @param[in] ptr - Pointer to the PLDM message
 * @param[in] hdr - Pointer to the PLDM header information
 *
 * @return Success code
 */
int pack_pldm_message(void *ptr, struct pldm_header_info *hdr);

/**
 * @brief Unpack the PLDM message and extract the header information and PLDM
 *        message payload. This function does not do a memory allocation.
 *
 * @param[in] ptr - Pointer to the PLDM message
 * @param[in] size - Size of the PLDM message
 * @param[out] hdr - Pointer to the PLDM header information
 * @param[out] offset - Offset to the PLDM payload
 * @param[out] payload_size - Size of the PLDM payload size
 *
 * @return Success code
 */
int unpack_pldm_message(void *ptr, size_t size, struct pldm_header_info *hdr,
                        size_t *offset, size_t *payload_size);

/** @brief API return codes
 */
enum error_codes {
	OK	=  0,
	FAIL	= -1
};

/** @brief PLDM base codes
 */
enum pldm_completion_codes {
	SUCCESS				= 0x00,
	ERROR				= 0x01,
	ERROR_INVALID_DATA		= 0x02,
	ERROR_INVALID_LENGTH		= 0x03,
	ERROR_NOT_READY			= 0x04,
	ERROR_UNSUPPORTED_PLDM_CMD	= 0x05,
	ERROR_INVALID_PLDM_TYPE		= 0x20
};

typedef uint32_t ver32;

#define REQUEST_HEADER_LEN_BYTES 3
#define RESPONSE_HEADER_LEN_BYTES 4

#define GET_TYPES_REQ_DATA_BYTES 0
#define GET_TYPES_RESP_DATA_BYTES 8
#define MAX_TYPES 64

#define GET_COMMANDS_REQ_DATA_BYTES 5
#define GET_COMMANDS_RESP_DATA_BYTES 32
#define MAX_CMDS_PER_TYPE 256

/* Requester */

/* GetPLDMTypes */

/** @brief Create a PLDM request message for GetPLDMTypes
 *
 *  @param instance_id[in] Message's instance id
 *  @param buffer[in/out] Message will be written to this buffer.
 *         Caller is responsible for memory alloc and dealloc.
 *  @return 0 on success, negative error code on failure
 */
int encode_get_types_req(uint8_t instance_id,
			 uint8_t *buffer);

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
int encode_get_commands_req(uint8_t instance_id,
			    uint8_t type,
			    ver32 version,
			    uint8_t *buffer);

/* Responder */

/* GetPLDMTypes */

/** @brief Create a PLDM response message for GetPLDMTypes
 *
 *  @param instance_id[in] Message's instance id
 *  @param completion_cpde[in] PLDM completion code
 *  @param types[in] pointer to array uint8_t[8] containing supported
 *         types (MAX_TYPES/sizeof(uint8_t) = 8), as per DSP0240.
 *  @param buffer[in/out] Message will be written to this buffer.
 *         Caller is responsible for memory alloc and dealloc.
 *  @return 0 on success, negative error code on failure
 */
int encode_get_types_resp(uint8_t instance_id,
			  uint8_t completion_code,
			  const uint8_t *types,
			  uint8_t *buffer);

/* GetPLDMCommands */

/** @brief Decode GetPLDMCommands' request data
 *
 *  @param payload[in] Request data
 *  @param type[out] PLDM Type
 *  @param version[out] Version for PLDM Type
 *  @return 0 on success, negative error code on failure
 */
int decode_get_commands_req(const uint8_t *request,
			    uint8_t *type,
			    ver32 *version);

/** @brief Create a PLDM response message for GetPLDMCommands
 *
 *  @param instance_id[in] Message's instance id
 *  @param completion_cpde[in] PLDM completion code
 *  @param types[in] pointer to array uint8_t[32] containing supported
 *         types (MAX_CMDS_PER_TYPE/sizeof(uint8_t) = 32), as per DSP0240.
 *  @param buffer[in/out] Message will be written to this buffer.
 *         Caller is responsible for memory alloc and dealloc.
 *  @return 0 on success, negative error code on failure
 */
int encode_get_commands_resp(uint8_t instance_id,
			     uint8_t completion_code,
			     const uint8_t *commands,
			     uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif /* BASE_H */
