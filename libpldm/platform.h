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

#define PLDM_GET_PDR_REQ_BYTES 13
/* Minimum response length */
#define PLDM_GET_PDR_MIN_RESP_BYTES 12

#define PLDM_GET_STATE_SENSOR_READINGS_REQ_BYTES 4
/* Maximum response length */
#define PLDM_GET_STATE_SENSOR_READINGS_RESP_MAX_BYTES 34
/* Minimum response length */
#define PLDM_GET_STATE_SENSOR_READINGS_RESP_MIN_BYTES 6

enum set_request { PLDM_NO_CHANGE = 0x00, PLDM_REQUEST_SET = 0x01 };

enum effecter_state { PLDM_INVALID_VALUE = 0xFF };

enum pldm_platform_commands {
	PLDM_SET_STATE_EFFECTER_STATES = 0x39,
	PLDM_GET_PDR = 0x51,
	PLDM_GET_STATE_SENSOR = 0x21,
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

/** @brief PLDM Platform M&C completion codes
 */
enum pldm_platform_completion_codes {
	PLDM_PLATFORM_INVALID_RECORD_HANDLE = 0x82,
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
	bool_t has_description_pdr;
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

/** @struct get_sensor_state_field
 *
 *  Structure representing a stateField in GetStateSensorReadings command */
typedef struct state_field_for_get_state_sensor {
	uint8_t sensor_op_state; //!< State of the sensor
	uint8_t present_state;   //!< Most recently assessed state
	uint8_t previous_state;  //!< State prior to present_state
	uint8_t event_state;     //!< Most recent state that caused an event
} __attribute__((packed)) get_sensor_state_field;

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
 *         memory prior to calling this command. Since the state field count is
 *         not known in advance, the user should allocate the maximum size
 *         always, which is 8 in number.
 *  @return pldm_completion_codes
 */
int decode_set_state_effecter_states_req(const struct pldm_msg_payload *msg,
					 uint16_t *effecter_id,
					 uint8_t *comp_effecter_count,
					 set_effecter_state_field *field);

/* GetPDR */

/** @brief Create a PLDM response message for GetPDR
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] next_record_hndl - The recordHandle for the PDR that is next in
 *        the PDR Repository
 *  @param[in] next_data_transfer_hndl - A handle that identifies the next
 *        portion of the PDR data to be transferred, if any
 *  @param[in] transfer_flag - Indicates the portion of PDR data being
 *        transferred
 *  @param[in] resp_cnt - The number of recordData bytes returned in this
 *        response
 *  @param[in] record_data - PDR data bytes of length resp_cnt
 *  @param[in] transfer_crc - A CRC-8 for the overall PDR. This is present only
 *        in the last part of a PDR being transferred
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */
int encode_get_pdr_resp(uint8_t instance_id, uint8_t completion_code,
			uint32_t next_record_hndl,
			uint32_t next_data_transfer_hndl, uint8_t transfer_flag,
			uint16_t resp_cnt, const uint8_t *record_data,
			uint8_t transfer_crc, struct pldm_msg *msg);

/** @brief Decode GetPDR request data
 *
 *  @param[in] msg - Request message payload
 *  @param[out] record_hndl - The recordHandle value for the PDR to be retrieved
 *  @param[out] data_transfer_hndl - Handle used to identify a particular
 *         multipart PDR data transfer operation
 *  @param[out] transfer_op_flag - Flag to indicate the first or subsequent
 *         portion of transfer
 *  @param[out] request_cnt - The maximum number of record bytes requested
 *  @param[out] record_chg_num - Used to determine whether the PDR has changed
 *        while PDR transfer is going on
 *  @return pldm_completion_codes
 */

int decode_get_pdr_req(const struct pldm_msg_payload *msg,
		       uint32_t *record_hndl, uint32_t *data_transfer_hndl,
		       uint8_t *transfer_op_flag, uint16_t *request_cnt,
		       uint16_t *record_chg_num);

/* GetStateSensorReadings */

/** @brief Create a PLDM response message for GetStateSensorReadings
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] comp_sensor_cnt - The number of individual sets of sensor data
 *  @param[in] field - Each stateField is an instance of a stateField structure
 *      equal to comp_sensor_cnt in number
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *      'msg.body.payload'
 */
int encode_get_state_sensor_readings_resp(uint8_t instance_id,
					  uint8_t completion_code,
					  uint8_t comp_sensor_cnt,
					  get_sensor_state_field *field,
					  struct pldm_msg *msg);

/** @brief Decode GetStateSensorReadings request packet
 *
 *  @param[in] msg - Request message payload
 *  @param[out] sensor_id - Unique identifier for the sensor
 *  @param[out] sensor_re_arm - Each bit location in this bit
 *      field corresponds to a particular sensor within the state sensor
 *  @param[out] reserved - Reserved
 */
int decode_get_state_sensor_readings_req(const struct pldm_msg_payload *msg,
					 uint16_t *sensor_id,
					 bitfield8_t *sensor_re_arm,
					 uint8_t *reserved);

/** @brief Encode GetPDR request data
 *  @param[in] instance_id - Message's instance id
 *  @param[in] record_handle –  The recordHandle value for the PDR to be encoded
 *  @param[in] data_transfer_handle –  Handle used to identify a particular
 * multipart PDR data transfer operation
 *  @param[in] transfer_operation_flag – Flag to indicate the first or
 * subsequent portion of transfer
 *  @param[in] request_count – The maximum number of record bytes requested
 *  @param[in] record_change_number – Used to determine whether the PDR has
 * change while PDR transfer is going on
 *  @param[out] msg – Message will be written to this
 */

int encode_get_pdr_req(uint8_t instance_id, uint32_t record_handle,
		       uint32_t data_transfer_handle,
		       uint8_t transfer_operation_flag, uint16_t request_count,
		       uint16_t record_change_number, struct pldm_msg *msg);

/** @brief Decode GetPDR response data
 *  @param[in] msg - Response message payload
 *  @param[out] completion_code –  PLDM completion code
 *  @param[out] next_record_handle –  The recordHandle for the PDR that is next
 * in the PDR Repository
 *  @param[out] next data_transfer_handle –  A handle that identifies the next
 * portion of the PDR data to be transferred, if any
 *  @param[out] transfer_flag – Indicates the portion of PDR data being
 * transferred
 *  @param[out] response_count – The number of recordData bytes to be decoded in
 * this response
 *  @param[out] record_data – PDR data bytes of length response_count
 *  @param[out] transfer_crc -  A CRC-8 for the overall PDR. This is present
 * only in the last part of a PDR being transferred
 */

int decode_get_pdr_resp(const struct pldm_msg_payload *msg,
			uint8_t *completion_code, uint32_t *next_record_handle,
			uint32_t *next_data_transfer_handle,
			uint8_t *transfer_flag, uint16_t *response_count,
			uint8_t *record_data, uint8_t *transfer_crc);

/* Requester */

/* SetStateEffecterStates */

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
