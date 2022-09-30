#include "base.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Store details of a supported type.
 */
struct pldm_type_details {
	uint8_t type;
	uint32_t version;
	uint8_t number_of_commands;
	const uint8_t *supported_command;
};

/**
 * @brief Keep track of all the PLDM types, versions and their commands
 * supported by the implementation.
 */
struct pldm_all_type_capabilities {
	const struct pldm_type_details *type[255];
};

/**
 * @brief Used to handle responder states.
 */
struct pldm_responder_state {
	bool used;
	bool in_progress;
	// PLDM terminus ID
	uint8_t tid;
	uint8_t requester_eid;
	uint8_t pldm_type;
	uint8_t command_code;
	uint8_t instance_id;
	const struct pldm_all_type_capabilities *capabilities;
};

/**
 * @brief Encapsulate PLDM response.
 */
struct pldm_responder_response {
	size_t len;
	void *payload;
};

bool base_responder_add_supported_type(
    struct pldm_all_type_capabilities *type_capabilities,
    enum pldm_supported_types type);
uint8_t pldm_base_handle_request(struct pldm_responder_state *state,
				 const struct pldm_msg *request,
				 size_t request_size,
				 struct pldm_responder_response *response);

#ifdef __cplusplus
}
#endif
