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

/** @struct set_effecter_state_field
 *
 *  Structure representing a stateField in SetStateEffecterStates command */

typedef struct state_field_for_state_effecter_set {
	uint8_t set_request;    //!< Whether to change the state
	uint8_t effecter_state; //!< Expected state of the effecter
} __attribute__((packed)) set_effecter_state_field;

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
 *  @param[in] msg - Request message payload
 *  @param[out] effecter_id - used to identify and access the effecter
 *  @param[out] comp_effecter_count - number of individual sets of effecter
 *         information. Upto eight sets of state effecter info can be accessed
 *         for a given effecter.
 *  @param[out] field - each unit is an instance of the stateFileld structure
 *         that is used to set the requested state for a particular effecter
 *         within the state effecter. This field holds the starting address of
 *         the stateField values. The user is responsible to allocate the
 *         memory prior to calling this command. The user has to allocate the
 *         state field count as sizeof(set_effecter_state_field) *
 *         comp_effecter_count
 *  @return pldm_completion_codes
 */
int decode_set_state_effecter_states_req(const struct pldm_msg_payload *msg,
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
 *         state field count as sizeof(set_effecter_state_field) *
 *         comp_effecter_count
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */

int encode_set_state_effecter_states_req(uint8_t instance_id,
					 uint16_t effecter_id,
					 uint8_t comp_effecter_count,
					 set_effecter_state_field *field,
					 struct pldm_msg *msg);

/** @brief Decode SetStateEffecterStates response data
 *  @param[in] msg - Request message payload
 *  @param[out] completion_code - PLDM completion code
 *  @return pldm_completion_codes
 */
int decode_set_state_effecter_states_resp(const struct pldm_msg_payload *msg,
					  uint8_t *completion_code);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
