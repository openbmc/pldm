#ifndef PLDM_BASE_REQUESTER_H
#define PLDM_BASE_REQUESTER_H

#ifdef __cplusplus
extern "C" {
#endif
#include "libpldm/base.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PLDM_TYPES 6
// LOCAL BUILD
typedef enum requester_status {
	PLDM_BASE_REQUESTER_SUCCESS = 0,
	PLDM_BASE_REQUESTER_NOT_PLDM_BASE_MSG = -1,
	PLDM_BASE_REQUESTER_NOT_RESP_MSG = -2,
	PLDM_BASE_REQUESTER_SEND_FAIL = -3,
	PLDM_BASE_REQUESTER_RECV_FAIL = -4,
	PLDM_BASE_REQUESTER_NO_NEXT_COMMAND_FOUND = -5,
	PLDM_BASE_REQUESTER_ENCODING_REQUEST_FAILURE = -6
} pldm_base_requester_rc_t;

enum command_status {
	COMMAND_FAILED = -1,
	COMMAND_COMPLETED = 0,
	COMMAND_NOT_STARTED = 1,
	COMMAND_WAITING = 2,
};

struct requester_base_context {
	bool initialized;
	uint8_t next_command;
	uint8_t command_status;
	uint8_t command_pldm_type; // do not have to initialize, while encoding
				   // the request just assign pldm_type
	uint8_t tid;
	uint8_t pldm_types[8];
	uint8_t pldm_commands[32];
	ver32_t pldm_versions[PLDM_MAX_TYPES];
};

/**
 * @brief Initializes the context for PLDM Base discovery commands
 *
 * @param[in] ctx - pointer to a context which is to be initialized
 * @param[in] eid - pointer to destination eid
 *
 * @return pldm_requester_rc_t (errno may be set)
 */
pldm_base_requester_rc_t pldm_base_start_discovery(struct requester_base_context *ctx);

/**
 * @brief Gets the next PLDM command from a request buffer to be processed
 *
 * @param[in] ctx - pointer to a context which is to be initialized
 * @param[in] request - *request is a pointer to the request that is present in
 * the buffer
 *
 * @return pldm_requester_rc_t (errno may be set)
 */
pldm_base_requester_rc_t
pldm_base_get_next_request(struct requester_base_context *ctx,
			   uint8_t instance_id, struct pldm_msg *request);

// TODO: Write Brief and implement
pldm_base_requester_rc_t
pldm_base_get_current_status(struct requester_base_context *ctx);

// TODO: Write Brief and implement
pldm_base_requester_rc_t pldm_base_push_response(struct requester_base_context *ctx,
					   void *resp_msg, size_t resp_size);

#ifdef __cplusplus
}
#endif

#endif /* PLDM_BASE_REQUESTER_H */