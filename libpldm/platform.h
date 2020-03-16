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
#define PLDM_GET_STATE_SENSOR_READINGS_REQ_BYTES 4
#define PLDM_GET_NUMERIC_EFFECTER_VALUE_REQ_BYTES 2
/* Response lengths are inclusive of completion code */
#define PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES 1
#define PLDM_GET_STATE_SENSOR_READINGS_RESP_BYTES 34

#define PLDM_SET_NUMERIC_EFFECTER_VALUE_RESP_BYTES 1
#define PLDM_SET_NUMERIC_EFFECTER_VALUE_MIN_REQ_BYTES 4
#define PLDM_GET_NUMERIC_EFFECTER_VALUE_MIN_RESP_BYTES 5

#define PLDM_GET_PDR_REQ_BYTES 13
/* Minimum response length */
#define PLDM_GET_PDR_MIN_RESP_BYTES 12

enum pldm_effecter_data_size {
	PLDM_EFFECTER_DATA_SIZE_UINT8,
	PLDM_EFFECTER_DATA_SIZE_SINT8,
	PLDM_EFFECTER_DATA_SIZE_UINT16,
	PLDM_EFFECTER_DATA_SIZE_SINT16,
	PLDM_EFFECTER_DATA_SIZE_UINT32,
	PLDM_EFFECTER_DATA_SIZE_SINT32
};

enum set_request { PLDM_NO_CHANGE = 0x00, PLDM_REQUEST_SET = 0x01 };

enum effecter_state { PLDM_INVALID_VALUE = 0xFF };

enum sensor_operational_state {
	ENABLED = 0x00,
	DISABLED = 0x01,
	UNAVAILABLE = 0x02,
	STATUSUNKNOWN = 0x03,
	FAILED = 0x04,
	INITIALIZING = 0x05,
	SHUTTINGDOWN = 0x06,
	INTEST = 0x07
};

enum present_state {
	UNKNOWN = 0x0,
	NORMAL = 0x01,
	WARNING = 0x02,
	CRITICAL = 0x03,
	FATAL = 0x04,
	LOWERWARNING = 0x05,
	LOWERCRITICAL = 0x06,
	LOWERFATAL = 0x07,
	UPPERWARNING = 0x08,
	UPPERCRITICAL = 0x09,
	UPPERFATAL = 0x0a
};

enum pldm_effecter_oper_state {
	EFFECTER_OPER_STATE_ENABLED_UPDATEPENDING,
	EFFECTER_OPER_STATE_ENABLED_NOUPDATEPENDING,
	EFFECTER_OPER_STATE_DISABLED,
	EFFECTER_OPER_STATE_UNAVAILABLE,
	EFFECTER_OPER_STATE_STATUSUNKNOWN,
	EFFECTER_OPER_STATE_FAILED,
	EFFECTER_OPER_STATE_INITIALIZING,
	EFFECTER_OPER_STATE_SHUTTINGDOWN,
	EFFECTER_OPER_STATE_INTEST
};

enum pldm_platform_commands {
	PLDM_GET_STATE_SENSOR_READINGS = 0x21,
	PLDM_SET_NUMERIC_EFFECTER_VALUE = 0x31,
	PLDM_GET_NUMERIC_EFFECTER_VALUE = 0x32,
	PLDM_SET_STATE_EFFECTER_STATES = 0x39,
	PLDM_GET_PDR = 0x51,
};

/** @brief PLDM PDR types
 */
enum pldm_pdr_types {
	PLDM_STATE_EFFECTER_PDR = 11,
	PLDM_PDR_FRU_RECORD_SET = 20,
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
	PLDM_PLATFORM_INVALID_EFFECTER_ID = 0x80,
	PLDM_PLATFORM_INVALID_STATE_VALUE = 0x81,
	PLDM_PLATFORM_INVALID_RECORD_HANDLE = 0x82,
	PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE = 0x82,
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

/** @struct pldm_pdr_fru_record_set
 *
 *  Structure representing PLDM FRU record set PDR
 */
struct pldm_pdr_fru_record_set {
	uint16_t terminus_handle;
	uint16_t fru_rsi;
	uint16_t entity_type;
	uint16_t entity_instance_num;
	uint16_t container_id;
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

/** @struct get_sensor_readings_field
 *
 *  Structure representing a stateField in GetStateSensorReadings command */

typedef struct state_field_for_get_state_sensor_readings {
	uint8_t sensor_op_state; //!< The state of the sensor itself
	uint8_t present_state;   //!< Return a state value
	uint8_t previous_state; //!< The state that the presentState was entered
				//! from. This must be different from the
				//! present state
	uint8_t event_state;    //!< Return a state value from a PLDM State Set
			     //! that is associated with the sensor
} __attribute__((packed)) get_sensor_state_field;

/** @struct PLDM_SetStateEffecterStates_Request
 *
 *  Structure representing PLDM set state effecter states request.
 */
struct pldm_set_state_effecter_states_req {
	uint16_t effecter_id;
	uint8_t comp_effecter_count;
	set_effecter_state_field field[8];
} __attribute__((packed));

/** @struct pldm_get_pdr_resp
 *
 *  structure representing GetPDR response packet
 *  transfer CRC is not part of the structure and will be
 *  added at the end of last packet in multipart transfer
 */
struct pldm_get_pdr_resp {
	uint8_t completion_code;
	uint32_t next_record_handle;
	uint32_t next_data_transfer_handle;
	uint8_t transfer_flag;
	uint16_t response_count;
	uint8_t record_data[1];
} __attribute__((packed));

/** @struct pldm_get_pdr_req
 *
 *  structure representing GetPDR request packet
 */
struct pldm_get_pdr_req {
	uint32_t record_handle;
	uint32_t data_transfer_handle;
	uint8_t transfer_op_flag;
	uint16_t request_count;
	uint16_t record_change_number;
} __attribute__((packed));

/** @struct pldm_set_numeric_effecter_value_req
 *
 *  structure representing SetNumericEffecterValue request packet
 */
struct pldm_set_numeric_effecter_value_req {
	uint16_t effecter_id;
	uint8_t effecter_data_size;
	uint8_t effecter_value[1];
} __attribute__((packed));

/** @struct pldm_get_state_sensor_readings_req
 *
 *  Structure representing PLDM get state sensor readings request.
 */
struct pldm_get_state_sensor_readings_req {
	uint16_t sensor_id;
	bitfield8_t sensor_rearm;
	uint8_t reserved;
} __attribute__((packed));

/** @struct pldm_get_state_sensor_readings_resp
 *
 *  Structure representing PLDM get state sensor readings response.
 */
struct pldm_get_state_sensor_readings_resp {
	uint8_t completion_code;
	uint8_t comp_sensor_count;
	get_sensor_state_field field[1];
} __attribute__((packed));

/** @struct pldm_get_numeric_effecter_value_req
 *
 *  structure representing GetNumericEffecterValue request packet
 */
struct pldm_get_numeric_effecter_value_req {
	uint16_t effecter_id;
} __attribute__((packed));

/** @struct pldm_get_numeric_effecter_value_resp
 *
 *  structure representing GetNumericEffecterValue response packet
 */
struct pldm_get_numeric_effecter_value_resp {
	uint8_t completion_code;
	uint8_t effecter_data_size;
	uint8_t effecter_oper_state;
	uint8_t pending_and_present_values[1];
} __attribute__((packed));

/* Responder */

/* SetNumericEffecterValue */

/** @brief Decode SetNumericEffecterValue request data
 *
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of request message payload
 *  @param[out] effecter_id - used to identify and access the effecter
 *  @param[out] effecter_data_size - The bit width and format of the setting
 * 				value for the effecter.
 * 				value:{uint8,sint8,uint16,sint16,uint32,sint32}
 *  @param[out] effecter_value - The setting value of numeric effecter being
 * 				requested.
 *  @return pldm_completion_codes
 */
int decode_set_numeric_effecter_value_req(const struct pldm_msg *msg,
					  size_t payload_length,
					  uint16_t *effecter_id,
					  uint8_t *effecter_data_size,
					  uint8_t *effecter_value);

/** @brief Create a PLDM response message for SetNumericEffecterValue
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[out] msg - Message will be written to this
 *  @param[in] payload_length - Length of request message payload
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */
int encode_set_numeric_effecter_value_resp(uint8_t instance_id,
					   uint8_t completion_code,
					   struct pldm_msg *msg,
					   size_t payload_length);

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
 *         'msg.payload'
 */
int encode_get_pdr_resp(uint8_t instance_id, uint8_t completion_code,
			uint32_t next_record_hndl,
			uint32_t next_data_transfer_hndl, uint8_t transfer_flag,
			uint16_t resp_cnt, const uint8_t *record_data,
			uint8_t transfer_crc, struct pldm_msg *msg);

/** @brief Decode GetPDR request data
 *
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of request message payload
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

int decode_get_pdr_req(const struct pldm_msg *msg, size_t payload_length,
		       uint32_t *record_hndl, uint32_t *data_transfer_hndl,
		       uint8_t *transfer_op_flag, uint16_t *request_cnt,
		       uint16_t *record_chg_num);

/* GetStateSensorReadings */

/** @brief Decode GetStateSensorReadings request data
 *
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of request message payload
 *  @param[out] sensor_id - used to identify and access the simple or composite
 *         sensor
 *  @param[out] sensor_rearm - Each bit location in this field corresponds to a
 *         particular sensor within the state sensor, where bit [0] corresponds
 *         to the first state sensor (sensor offset 0) and bit [7] corresponds
 *         to the eighth sensor (sensor offset 7), sequentially.
 *  @param[out] reserved - value: 0x00
 *  @return pldm_completion_codes
 */

int decode_get_state_sensor_readings_req(const struct pldm_msg *msg,
					 size_t payload_length,
					 uint16_t *sensor_id,
					 bitfield8_t *sensor_rearm,
					 uint8_t *reserved);

/** @brief Encode GetStateSensorReadings response data
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[out] comp_sensor_count - The number of individual sets of sensor
 *         information that this command accesses
 *  @param[out] field - Each stateField is an instance of a stateField structure
 *         that is used to return the present operational state setting and the
 *         present state and event state for a particular set of sensor
 *         information contained within the state sensor
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 */

int encode_get_state_sensor_readings_resp(uint8_t instance_id,
					  uint8_t completion_code,
					  uint8_t comp_sensor_count,
					  get_sensor_state_field *field,
					  struct pldm_msg *msg);

/* GetNumericEffecterValue */

/** @brief Decode GetNumericEffecterValue request data
 *
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of request message payload
 *  @param[out] effecter_id - used to identify and access the effecter
 *  @return pldm_completion_codes
 */
int decode_get_numeric_effecter_value_req(const struct pldm_msg *msg,
					  size_t payload_length,
					  uint16_t *effecter_id);

/** @brief Create a PLDM response message for GetNumericEffecterValue
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] effecter_data_size - The bit width and format of the setting
 *             value for the effecter.
 * 	       value:{uint8,sint8,uint16,sint16,uint32,sint32}
 *  @param[in] effecter_oper_state - The state of the effecter itself
 *  @param[in] pending_value - The pending numeric value setting of the
 *             effecter. The effecterDataSize field indicates the number of
 *             bits used for this field
 *  @param[in] present_value - The present numeric value setting of the
 *             effecter. The effecterDataSize indicates the number of bits
 *             used for this field
 *  @param[out] msg - Message will be written to this
 *  @param[in] payload_length - Length of response message payload
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_get_numeric_effecter_value_resp(
    uint8_t instance_id, uint8_t completion_code, uint8_t effecter_data_size,
    uint8_t effecter_oper_state, uint8_t *pending_value, uint8_t *present_value,
    struct pldm_msg *msg, size_t payload_length);

/* Requester */

/* GetPDR */

/** @brief Create a PLDM request message for GetPDR
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] record_hndl - The recordHandle value for the PDR to be retrieved
 *  @param[in] data_transfer_hndl - Handle used to identify a particular
 *         multipart PDR data transfer operation
 *  @param[in] transfer_op_flag - Flag to indicate the first or subsequent
 *         portion of transfer
 *  @param[in] request_cnt - The maximum number of record bytes requested
 *  @param[in] record_chg_num - Used to determine whether the PDR has changed
 *        while PDR transfer is going on
 *  @param[out] msg - Message will be written to this
 *  @param[in] payload_length - Length of request message payload
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_get_pdr_req(uint8_t instance_id, uint32_t record_hndl,
		       uint32_t data_transfer_hndl, uint8_t transfer_op_flag,
		       uint16_t request_cnt, uint16_t record_chg_num,
		       struct pldm_msg *msg, size_t payload_length);

/** @brief Decode GetPDR response data
 *
 *  Note:
 *  * If the return value is not PLDM_SUCCESS, it represents a
 * transport layer error.
 *  * If the completion_code value is not PLDM_SUCCESS, it represents a
 * protocol layer error and all the out-parameters are invalid.
 *
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of request message payload
 *  @param[out] completion_code - PLDM completion code
 *  @param[out] next_record_hndl - The recordHandle for the PDR that is next in
 *        the PDR Repository
 *  @param[out] next_data_transfer_hndl - A handle that identifies the next
 *        portion of the PDR data to be transferred, if any
 *  @param[out] transfer_flag - Indicates the portion of PDR data being
 *        transferred
 *  @param[out] resp_cnt - The number of recordData bytes returned in this
 *        response
 *  @param[out] record_data - PDR data bytes of length resp_cnt
 *  @param[in] record_data_length - Length of record_data
 *  @param[out] transfer_crc - A CRC-8 for the overall PDR. This is present only
 *        in the last part of a PDR being transferred
 *  @return pldm_completion_codes
 */
int decode_get_pdr_resp(const struct pldm_msg *msg, size_t payload_length,
			uint8_t *completion_code, uint32_t *next_record_hndl,
			uint32_t *next_data_transfer_hndl,
			uint8_t *transfer_flag, uint16_t *resp_cnt,
			uint8_t *record_data, size_t record_data_length,
			uint8_t *transfer_crc);

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
 *         'msg.payload'
 */

int encode_set_state_effecter_states_req(uint8_t instance_id,
					 uint16_t effecter_id,
					 uint8_t comp_effecter_count,
					 set_effecter_state_field *field,
					 struct pldm_msg *msg);

/** @brief Decode SetStateEffecterStates response data
 *
 *  Note:
 *  * If the return value is not PLDM_SUCCESS, it represents a
 * transport layer error.
 *  * If the completion_code value is not PLDM_SUCCESS, it represents a
 * protocol layer error and all the out-parameters are invalid.
 *
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - PLDM completion code
 *  @return pldm_completion_codes
 */
int decode_set_state_effecter_states_resp(const struct pldm_msg *msg,
					  size_t payload_length,
					  uint8_t *completion_code);

/* SetNumericEffecterValue */

/** @brief Create a PLDM request message for SetNumericEffecterValue
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] effecter_id - used to identify and access the effecter
 *  @param[in] effecter_data_size - The bit width and format of the setting
 * 				value for the effecter.
 * 				value:{uint8,sint8,uint16,sint16,uint32,sint32}
 *  @param[in] effecter_value - The setting value of numeric effecter being
 * 				requested.
 *  @param[in] payload_length - Length of request message payload
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_set_numeric_effecter_value_req(
    uint8_t instance_id, uint16_t effecter_id, uint8_t effecter_data_size,
    uint8_t *effecter_value, struct pldm_msg *msg, size_t payload_length);

/** @brief Decode SetNumericEffecterValue response data
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - PLDM completion code
 *  @return pldm_completion_codes
 */
int decode_set_numeric_effecter_value_resp(const struct pldm_msg *msg,
					   size_t payload_length,
					   uint8_t *completion_code);

/** @brief Create a PLDM request message for GetStateSensorReadings
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] sensor_id - used to identify and access the simple or composite
 *         sensor
 *  @param[in] sensorRearm - Each bit location in this field corresponds to a
 *         particular sensor within the state sensor, where bit [0] corresponds
 *         to the first state sensor (sensor offset 0) and bit [7] corresponds
 *         to the eighth sensor (sensor offset 7), sequentially
 *  @param[in] reserved - value: 0x00
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */

int encode_get_state_sensor_readings_req(uint8_t instance_id,
					 uint16_t sensor_id,
					 bitfield8_t sensor_rearm,
					 uint8_t reserved,
					 struct pldm_msg *msg);

/** @brief Decode GetStateSensorReadings response data
 *
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - PLDM completion code
 *  @param[in,out] comp_sensor_count - The number of individual sets of sensor
 *         information that this command accesses
 *  @param[out] field - Each stateField is an instance of a stateField structure
 *         that is used to return the present operational state setting and the
 *         present state and event state for a particular set of sensor
 *         information contained within the state sensor
 *  @return pldm_completion_codes
 */

int decode_get_state_sensor_readings_resp(const struct pldm_msg *msg,
					  size_t payload_length,
					  uint8_t *completion_code,
					  uint8_t *comp_sensor_count,
					  get_sensor_state_field *field);

/* GetNumericEffecterValue */

/** @brief Create a PLDM request message for GetNumericEffecterValue
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] effecter_id - used to identify and access the effecter
 *  @param[out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_get_numeric_effecter_value_req(uint8_t instance_id,
					  uint16_t effecter_id,
					  struct pldm_msg *msg);

/** @brief Create a PLDM response message for GetNumericEffecterValue
 *
 *  @param[in] msg - Request message
 *  @param[in] payload_length - Length of request message payload
 *  @param[out] completion_code - PLDM completion code
 *  @param[out] effecter_data_size - The bit width and format of the setting
 *		value for the effecter.
 *		value:{uint8,sint8,uint16,sint16,uint32,sint32}
 *  @param[out] effecter_oper_state - The state of the effecter itself
 *  @param[out] pending_value - The pending numeric value setting of the
 *              effecter. The effecterDataSize field indicates the number of
 *              bits used for this field
 *  @param[out] present_value - The present numeric value setting of the
 *              effecter. The effecterDataSize indicates the number of bits
 *              used for this field
 *  @return pldm_completion_codes
 */
int decode_get_numeric_effecter_value_resp(
    const struct pldm_msg *msg, size_t payload_length, uint8_t *completion_code,
    uint8_t *effecter_data_size, uint8_t *effecter_oper_state,
    uint8_t *pending_value, uint8_t *present_value);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
