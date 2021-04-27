#ifndef FW_UPDATE_H
#define FW_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif
#include "base.h"
#include "utils.h"

#define PLDM_FWU_BASELINE_TRANSFER_SIZE 32
#define PLDM_MIN_OUTSTANDING_REQ 1
#define PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES 0
/** @brief Minimum length of device descriptor, 2 bytes for descriptor type,
 *         2 bytes for descriptor length and atleast 1 byte of descriptor data
 */
#define PLDM_FWUP_DEVICE_DESCRIPTOR_MIN_LEN 5
#define PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES 0

/* Maximum progress percentage value*/
#define FW_UPDATE_MAX_PROGRESS_PERCENT 0x65

/** @brief PLDM Firmware update inventory commands
 */
enum pldm_firmware_update_inventory_commands {
	PLDM_QUERY_DEVICE_IDENTIFIERS = 0x01,
	PLDM_GET_FIRMWARE_PARAMETERS = 0x02
};

/** @brief PLDM Firmware update commands
 */
enum pldm_firmware_update_commands {
	PLDM_REQUEST_UPDATE = 0x10,
	PLDM_PASS_COMPONENT_TABLE = 0x13,
	PLDM_UPDATE_COMPONENT = 0x14,
	PLDM_GET_STATUS = 0x1B,
	PLDM_CANCEL_UPDATE = 0x1D
};

/** @brief PLDM FWU values for Component Version String Type or Component Image
 * Set Version String Type
 */
enum comp_type {
	PLDM_COMP_VER_STR_TYPE_UNKNOWN = 0,
	PLDM_COMP_ASCII = 1,
	PLDM_COMP_UTF_8 = 2,
	PLDM_COMP_UTF_16 = 3,
	PLDM_COMP_UTF_16LE = 4,
	PLDM_COMP_UTF_16BE = 5
};

/** @brief PLDM FWU common values for Component Response Code and Component
 * Compatibility Response Code
 */
enum pldm_comp_code {
	COMP_CAN_BE_UPDATED = 0x00,
	COMP_COMPARISON_STAMP_IDENTICAL = 0x01,
	COMP_COMPARISON_STAMP_LOWER = 0x02,
	INVALID_COMP_COMPARISON_STAMP = 0x03,
	COMP_CONFLICT = 0x04,
	COMP_PREREQUISITES = 0x05,
	COMP_NOT_SUPPORTED = 0x06,
	COMP_SECURITY_RESTRICTIONS = 0x07,
	INCOMPLETE_COMP_IMAGE_SET = 0x08,
	FD_DOWN_STREAM_DEVICE_NOT_UPDATE_SUBSEQUENTLY = 0x9,
	COMP_VER_STR_IDENTICAL = 0x0A,
	COMP_VER_STR_LOWER = 0x0B,
	FD_VENDOR_COMP_STATUS_CODE_RANGE_MIN = 0xD0,
	FD_VENDOR_COMP_STATUS_CODE_RANGE_MAX = 0xEF
};

/** @brief PLDM FWU codes for Component Response
 */
enum comp_resp {
	PLDM_COMP_CAN_BE_UPDATEABLE = 0,
	PLDM_COMP_MAY_BE_UPDATEABLE = 1
};

/** @brief PLDM FWU values for Component Classification
 */
enum comp_classification {
	COMP_UNKNOWN = 0x0000,
	COMP_OTHER = 0x0001,
	COMP_DRIVER = 0x0002,
	COMP_CONFIGURATION_SOFTWARE = 0x0003,
	COMP_APPLICATION_SOFTWARE = 0x0004,
	COMP_INSTRUMENTATION = 0x0005,
	COMP_FIRMWARE_OR_BIOS = 0x0006,
	COMP_DIAGNOSTIC_SOFTWARE = 0x0007,
	COMP_OPERATING_SYSTEM = 0x0008,
	COMP_MIDDLEWARE = 0x0009,
	COMP_FIRMWARE = 0x000A,
	COMP_BIOS_OR_FCODE = 0x000B,
	COMP_SUPPORT_OR_SERVICEPACK = 0x000C,
	COMP_SOFTWARE_BUNDLE = 0x000D,
	COMP_DOWNSTREAM_DEVICE = 0xffff
};

/** @brief PLDM FWU codes for component compatibility response code
 */
enum comp_compatability_resp_code {
	NO_RESPONSE_CODE = 0x0,
	COMPATABILITY_NO_MATCH = 0x09
};

/** @brief PLDM FWU codes for Component Compatibility Response
 */
enum comp_compatability_resp {
	COMPONENT_CAN_BE_UPDATED = 0,
	COMPONENT_CANNOT_BE_UPDATED = 1
};

/** @brief PLDM Firmware Update current or previous different states
 */
enum pldm_firmware_update_state {
	FD_IDLE = 0,
	FD_LEARN_COMPONENTS = 1,
	FD_READY_XFER = 2,
	FD_DOWNLOAD = 3,
	FD_VERIFY = 4,
	FD_APPLY = 5,
	FD_ACTIVATE = 6
};

/** @brief PLDM Firmware Update aux state
 */
enum pldm_firmware_update_aux_state {
	FD_OPERATION_IN_PROGRESS = 0,
	FD_OPERATION_SUCCESSFUL = 1,
	FD_OPERATION_FAILED = 2,
	FD_WAIT = 3
};

/** @brief PLDM Firmware Update aux state status
 */
enum pldm_firmware_update_aux_state_status {
	FD_AUX_STATE_IN_PROGRESS_OR_SUCCESS = 0x00,
	FD_TIMEOUT = 0x09,
	FD_GENERIC_ERROR = 0x0A,
	FD_VENDOR_DEFINED_STATUS_CODE_START = 0x70,
	FD_VENDOR_DEFINED_STATUS_CODE_END = 0xEF
};

/** @brief PLDM Firmware Update reason code
 */
enum pldm_firmware_update_reason_code {
	FD_INITIALIZATION = 0,
	FD_ACTIVATE_FW_RECEIVED = 1,
	FD_CANCEL_UPDATE_RECEIVED = 2,
	FD_TIMEOUT_LEARN_COMPONENT = 3,
	FD_TIMEOUT_READY_XFER = 4,
	FD_TIMEOUT_DOWNLOAD = 5,
	FD_TIMEOUT_VERIFY = 6,
	FD_TIMEOUT_APPLY = 7,
	FD_STATUS_VENDOR_DEFINED_MIN = 200,
	FD_STATUS_VENDOR_DEFINED_MAX = 255
};

/** @brief PLDM FWU codes for non functioning component indication
 */
enum non_functioning_component_indication {
	COMPONENTS_FUNCTIONING = 0,
	COMPONENTS_NOT_FUNCTIONING = 1
};

/** @struct pldm_query_device_identifiers_resp
 *
 *  Structure representing query device identifiers response.
 */
struct pldm_query_device_identifiers_resp {
	uint8_t completion_code;
	uint32_t device_identifiers_len;
	uint8_t descriptor_count;
} __attribute__((packed));

/** @struct pldm_get_firmware_parameters_resp
 *
 *  Structure representing get firmware parameters response.
 */
struct pldm_get_firmware_parameters_resp {
	uint8_t completion_code;
	bitfield32_t capabilities_during_update;
	uint16_t comp_count;
	uint8_t active_comp_image_set_ver_str_type;
	uint8_t active_comp_image_set_ver_str_len;
	uint8_t pending_comp_image_set_ver_str_type;
	uint8_t pending_comp_image_set_ver_str_len;
} __attribute__((packed));

/** @struct pldm_component_parameter_entry
 *
 *  Structure representing component parameter table entry.
 */
struct pldm_component_parameter_entry {
	uint16_t comp_classification;
	uint16_t comp_identifier;
	uint8_t comp_classification_index;
	uint32_t active_comp_comparison_stamp;
	uint8_t active_comp_ver_str_type;
	uint8_t active_comp_ver_str_len;
	uint8_t active_comp_release_date[8];
	uint32_t pending_comp_comparison_stamp;
	uint8_t pending_comp_ver_str_type;
	uint8_t pending_comp_ver_str_len;
	uint8_t pending_comp_release_date[8];
	bitfield16_t comp_activation_methods;
	bitfield32_t capabilities_during_update;
} __attribute__((packed));

/** @struct request_update_req
 *
 *  Structure representing Request Update request
 */
struct pldm_request_update_req {
	uint32_t max_transfer_size;
	uint16_t no_of_comp;
	uint8_t max_outstand_transfer_req;
	uint16_t pkg_data_len;
	uint8_t comp_image_set_ver_str_type;
	uint8_t comp_image_set_ver_str_len;
} __attribute__((packed));

/** @struct request_update_resp
 *
 *  Structure representing Request Update response
 */
struct pldm_request_update_resp {
	uint8_t completion_code;
	uint16_t fd_meta_data_len;
	uint8_t fd_pkg_data;
} __attribute__((packed));

/** @struct pass_component_table_req
 *
 *  Structure representing Pass Component Table Request
 */
struct pldm_pass_component_table_req {
	uint8_t transfer_flag;
	uint16_t comp_classification;
	uint16_t comp_identifier;
	uint8_t comp_classification_index;
	uint32_t comp_comparison_stamp;
	uint8_t comp_ver_str_type;
	uint8_t comp_ver_str_len;
} __attribute__((packed));

/** @struct pass_component_table_resp
 *
 *  Structure representing Pass Component Table response
 */
struct pldm_pass_component_table_resp {
	uint8_t completion_code;
	uint8_t comp_resp;
	uint8_t comp_resp_code;
} __attribute__((packed));

/** @struct update_component_req
 *
 *  Structure representing Update Component request
 */
struct pldm_update_component_req {
	uint16_t comp_classification;
	uint16_t comp_identifier;
	uint8_t comp_classification_index;
	uint32_t comp_comparison_stamp;
	uint32_t comp_image_size;
	bitfield32_t update_option_flags;
	uint8_t comp_ver_str_type;
	uint8_t comp_ver_str_len;
} __attribute__((packed));

/** @struct update_component_resp
 *
 *  Structure representing Update Component response
 */
struct pldm_update_component_resp {
	uint8_t completion_code;
	uint8_t comp_compatability_resp;
	uint8_t comp_compatability_resp_code;
	bitfield32_t update_option_flags_enabled;
	uint16_t estimated_time_req_fd;
} __attribute__((packed));

/** @struct get_status_resp
 *
 *  Structure representing GetStatus response.
 */
struct pldm_get_status_resp {
	uint8_t completion_code;
	uint8_t current_state;
	uint8_t previous_state;
	uint8_t aux_state;
	uint8_t aux_state_status;
	uint8_t progress_percent;
	uint8_t reason_code;
	bitfield32_t update_option_flags_enabled;
} __attribute__((packed));

/** @struct cancel_update_resp
 *
 *  Structure representing CancelUpdate response.
 */
struct pldm_cancel_update_resp {
	uint8_t completion_code;
	bool8_t non_functioning_component_indication;
	uint64_t non_functioning_component_bitmap;
} __attribute__((packed));

/** @brief Create a PLDM request message for QueryDeviceIdentifiers
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] payload_length - Length of the request message payload
 *  @param[in,out] msg - Message will be written to this
 *
 *  @return pldm_completion_codes
 *
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_query_device_identifiers_req(uint8_t instance_id,
					size_t payload_length,
					struct pldm_msg *msg);

/** @brief Decode QueryDeviceIdentifiers response message
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] device_identifiers_len - Pointer to device identifiers length
 *  @param[out] descriptor_count - Pointer to descriptor count
 *  @param[out] descriptor_data - Pointer to descriptor data
 *
 *  @return pldm_completion_codes
 */
int decode_query_device_identifiers_resp(const struct pldm_msg *msg,
					 size_t payload_length,
					 uint8_t *completion_code,
					 uint32_t *device_identifiers_len,
					 uint8_t *descriptor_count,
					 uint8_t **descriptor_data);

/** @brief Create a PLDM request message for GetFirmwareParameters
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] payload_length - Length of the request message payload
 *  @param[in,out] msg - Message will be written to this
 *
 *  @return pldm_completion_codes
 *
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_get_firmware_parameters_req(uint8_t instance_id,
				       size_t payload_length,
				       struct pldm_msg *msg);

/** @brief Decode GetFirmwareParameters response parameters except the
 *         ComponentParameterTable
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] resp_data - Pointer to get firmware parameters response
 *  @param[out] active_comp_image_set_ver_str - Pointer to active component
 * image set version string
 *  @param[out] pending_comp_image_set_ver_str - Pointer to pending component
 * image set version string
 *
 *  @return pldm_completion_codes
 */
int decode_get_firmware_parameters_resp_comp_set_info(
    const struct pldm_msg *msg, size_t payload_length,
    struct pldm_get_firmware_parameters_resp *resp_data,
    struct variable_field *active_comp_image_set_ver_str,
    struct variable_field *pending_comp_image_set_ver_str);

/** @brief Decode component entries in the component parameter table which is
 *         part of the response of GetFirmwareParameters command
 *
 *  @param[in] data - Component entry
 *  @param[in] length - Length of component entry
 *  @param[out] component_data - Pointer to component parameter table
 *  @param[out] active_comp_ver_str - Pointer to active component version string
 *  @param[out] pending_comp_ver_str - Pointer to pending component version
 *                                     string
 *
 *  @return pldm_completion_codes
 */
int decode_get_firmware_parameters_resp_comp_entry(
    const uint8_t *data, size_t length,
    struct pldm_component_parameter_entry *component_data,
    struct variable_field *active_comp_ver_str,
    struct variable_field *pending_comp_ver_str);

/** @brief Create a PLDM request message for RequestUpdate
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @param[in] payload_length - Length of request message payload
 *  @param[in] data - Pointer for RequestUpdate Request
 *  @param[in] comp_img_set_ver_str - Pointer which holds image set
 * information
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_request_update_req(const uint8_t instance_id, struct pldm_msg *msg,
			      const size_t payload_length,
			      const struct pldm_request_update_req *data,
			      struct variable_field *comp_img_set_ver_str);

/** @brief Decode a RequestUpdate response message
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] fd_meta_data_len - Pointer which holds length of FD meta data
 *  @param[out] fd_pkg_data - Pointer which holds package data
 * information
 *  @return pldm_completion_codes
 */
int decode_request_update_resp(const struct pldm_msg *msg,
			       const size_t payload_length,
			       uint8_t *completion_code,
			       uint16_t *fd_meta_data_len,
			       uint8_t *fd_pkg_data);

/** @brief Create a PLDM request message for PassComponentTable
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @param[in] payload_length - Length of request message payload
 *  @param[in] data - Pointer for PassComponentTable Request
 *  @param[in] comp_ver_str - Pointer to component version string
 * information
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_pass_component_table_req(
    const uint8_t instance_id, struct pldm_msg *msg,
    const size_t payload_length,
    const struct pldm_pass_component_table_req *data,
    struct variable_field *comp_ver_str);

/** @brief Decode a PassComponentTable response message
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] comp_resp - Pointer to component response
 *  @param[out] comp_resp_code - Pointer to component response code
 * information
 *  @return pldm_completion_codes
 */
int decode_pass_component_table_resp(const struct pldm_msg *msg,
				     const size_t payload_length,
				     uint8_t *completion_code,
				     uint8_t *comp_resp,
				     uint8_t *comp_resp_code);

/** @brief Create a PLDM request message for UpdateComponent
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @param[in] payload_length - Length of request message payload
 *  @param[in] data - Pointer for UpdateComponent Request
 *  @param[in] comp_ver_str - Pointer to component version string
 * information
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 * 'msg.payload'
 */
int encode_update_component_req(const uint8_t instance_id, struct pldm_msg *msg,
				const size_t payload_length,
				const struct pldm_update_component_req *data,
				struct variable_field *comp_ver_str);

/** @brief Decode a UpdateComponent response message
 *
 *  Note:
 *  * If the return value is not PLDM_SUCCESS, it represents a
 * transport layer error.
 *  * If the completion_code value is not PLDM_SUCCESS, it represents a
 * protocol layer error and all the out-parameters are invalid.
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] comp_compatability_resp - Pointer to component compatability
 * response
 *  @param[out] comp_compatability_resp_code - Pointer to component
 * compatability response code
 *  @param[out] update_option_flags_enabled - Pointer to update option flags
 * enabled
 *  @param[out] estimated_time_req_fd - Pointer to estimated time before sending
 * request firmware data information
 *  @return pldm_completion_codes
 */
int decode_update_component_resp(const struct pldm_msg *msg,
				 const size_t payload_length,
				 uint8_t *completion_code,
				 uint8_t *comp_compatability_resp,
				 uint8_t *comp_compatability_resp_code,
				 bitfield32_t *update_option_flags_enabled,
				 uint16_t *estimated_time_req_fd);

/** @brief Create a PLDM request message for GetStatus
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 * 'msg.payload'
 */
int encode_get_status_req(const uint8_t instance_id, struct pldm_msg *msg);

/** @brief Decode a GetStatus response message
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] current_state - Pointer to current state machine state
 *  @param[out] previous_state - Pointer to previous different state machine
 *state
 *  @param[out] aux_state - Pointer to current operation state
 *  @param[out] aux_state_status - Pointer to aux state status
 *  @param[out] progress_percent - Pointer to current progress percentage
 *  @param[out] reason_code - Pointer to reason for entering current state
 *  @param[out] update_option_flags_enabled - Pointer to update option flags
 *enabled
 *  @return pldm_completion_codes
 */
int decode_get_status_resp(const struct pldm_msg *msg,
			   const size_t payload_length,
			   uint8_t *completion_code, uint8_t *current_state,
			   uint8_t *previous_state, uint8_t *aux_state,
			   uint8_t *aux_state_status, uint8_t *progress_percent,
			   uint8_t *reason_code,
			   bitfield32_t *update_option_flags_enabled);

/** @brief Create a PLDM request message for CancelUpdate
 *
 *	@param[in] instance_id - Message's instance id
 *	@param[in,out] msg - Message will be written to this
 *	@return pldm_completion_codes
 *	@note  Caller is responsible for memory alloc and dealloc of param
 *'msg.payload'
 */
int encode_cancel_update_req(const uint8_t instance_id, struct pldm_msg *msg);

/** @brief Decode CancelUpdate response message
 *
 *	@param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *	@param[out] completion_code - Pointer to response msg's PLDM completion
 *code
 *	@param[out] non_functioning_component_indication - Pointer to non
 *funcional component indication
 *	@param[out] non_functioning_component_bitmap - Pointer to non functional
 *component bitmap state
 *	@return pldm_completion_codes
 */
int decode_cancel_update_resp(const struct pldm_msg *msg,
			      const size_t payload_len,
			      uint8_t *completion_code,
			      bool8_t *non_functioning_component_indication,
			      bitfield64_t *non_functioning_component_bitmap);

#ifdef __cplusplus
}
#endif

#endif // End of FW_UPDATE_H