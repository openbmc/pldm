#ifndef BASE_H
#define BASE_H

#ifdef __cplusplus
extern "C" {
#endif

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
 *         types (MAX_TYPES/sizeof(uint8_t) = 8), as per DSP0240.
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
 *         types (MAX_CMDS_PER_TYPE/sizeof(uint8_t) = 32), as per DSP0240.
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
