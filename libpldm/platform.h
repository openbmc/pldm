#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "base.h"

/* Maximum size for request */
#define PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES 19
/* Response lengths are inclusive of completion code */
#define PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES 1

enum set_request { PLDM_NO_CHANGE = 0x00, PLDM_REQUEST_SET = 0x01 };

enum effecter_state { PLDM_INVALID_VALUE = 0xFF };

enum pldm_platform_commands {
	PLDM_SET_STATE_EFFECTER_STATES = 0x39,
};

/** @brief PLDM PDR types
 */
enum pldm_pdr_types {
	PLDM_STATE_EFFECTER_PDR = 11,
};

/** @brief PLDM effecter initialization schemes
 */
enum pldm_effecter_init {
	PLDM_NO_INIT,
	PLDM_USE_INIT_PDR,
	PLDM_ENABLE_EFFECTER,
	PLDM_DISABLE_EFECTER
};

/** @struct pldm_pdr_hdr
 *
 *  Structure representing PLDM common PDR header
 */
struct pldm_pdr_hdr {
	uint32_t record_handle;
	uint8_t version;
	uint8_t type;
	uint16_t record_change_num;
	uint16_t length;
} __attribute__((packed));

/** @struct pldm_state_effecter_pdr
 *
 *  Structure representing PLDM state effecter PDR
 */
struct pldm_state_effecter_pdr {
	struct pldm_pdr_hdr hdr;
	uint16_t terminus_handle;
	uint16_t effecter_id;
	uint16_t entity_type;
	uint16_t entity_instance;
	uint16_t container_id;
	uint16_t effecter_semantic_id;
	uint8_t effecter_init;
	bool8_t has_description_pdr;
	uint8_t composite_effecter_count;
	uint8_t possible_states[1];
} __attribute__((packed));

/** @struct state_effecter_possible_states
 *
 *  Structure representing state enums for state effecter
 */
struct state_effecter_possible_states {
	uint16_t state_set_id;
	uint8_t possible_states_size;
	bitfield8_t states[1];
} __attribute__((packed));

/** @struct set_effecter_state_field
 *
 *  Structure representing a stateField in SetStateEffecterStates command */

typedef struct state_field_for_state_effecter_set {
	uint8_t set_request;    //!< Whether to change the state
	uint8_t effecter_state; //!< Expected state of the effecter
} __attribute__((packed)) set_effecter_state_field;

/** @struct PLDM_SetStateEffecterStates_Request
 *
 *  Structure representing PLDM set state effecter states request.
 */
struct pldm_set_state_effecter_states_req {
	uint16_t effecter_id;
	uint8_t comp_effecter_count;
	set_effecter_state_field field[8];
} __attribute__((packed));

/* Responder */

/* SetStateEffecterStates */

/** @brief Create a PLDM response message for SetStateEffecterStates
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */

int encode_set_state_effecter_states_resp(uint8_t instance_id,
					  uint8_t completion_code,
					  struct pldm_msg *msg);

/** @brief Decode SetStateEffecterStates request data
 *
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of request message payload
 *  @param[out] effecter_id - used to identify and access the effecter
 *  @param[out] comp_effecter_count - number of individual sets of effecter
 *         information. Upto eight sets of state effecter info can be accessed
 *         for a given effecter.
 *  @param[out] field - each unit is an instance of the stateFileld structure
 *         that is used to set the requested state for a particular effecter
 *         within the state effecter. This field holds the starting address of
 *         the stateField values. The user is responsible to allocate the
 *         memory prior to calling this command. Since the state field count is
 *         not known in advance, the user should allocate the maximum size
 *         always, which is 8 in number.
 *  @return pldm_completion_codes
 */

int decode_set_state_effecter_states_req(const struct pldm_msg *msg,
					 size_t payload_length,
					 uint16_t *effecter_id,
					 uint8_t *comp_effecter_count,
					 set_effecter_state_field *field);

/** @brief Create a PLDM request message for SetStateEffecterStates
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] effecter_id - used to identify and access the effecter
 *  @param[in] comp_effecter_count - number of individual sets of effecter
 *         information. Upto eight sets of state effecter info can be accessed
 *         for a given effecter.
 *  @param[in] field - each unit is an instance of the stateField structure
 *         that is used to set the requested state for a particular effecter
 *         within the state effecter. This field holds the starting address of
 *         the stateField values. The user is responsible to allocate the
 *         memory prior to calling this command. The user has to allocate the
 *         field parameter as sizeof(set_effecter_state_field) *
 *         comp_effecter_count
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */

int encode_set_state_effecter_states_req(uint8_t instance_id,
					 uint16_t effecter_id,
					 uint8_t comp_effecter_count,
					 set_effecter_state_field *field,
					 struct pldm_msg *msg);

/** @brief Decode SetStateEffecterStates response data
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - PLDM completion code
 *  @return pldm_completion_codes
 */
int decode_set_state_effecter_states_resp(const struct pldm_msg *msg,
					  size_t payload_length,
					  uint8_t *completion_code);
#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
