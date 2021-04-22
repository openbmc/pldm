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
#define PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES 0
// descriptor type 2 byte, length 2 bytes and data 1 byte min.
#define PLDM_FWU_MIN_DESCRIPTOR_IDENTIFIERS_LEN 5

/* inventory commands */
enum pldm_fwu_inventory_commands {
	PLDM_QUERY_DEVICE_IDENTIFIERS = 0x01,
	PLDM_GET_FIRMWARE_PARAMETERS = 0x02
};

/* update commands */
enum pldm_fwu_update_commands {
	PLDM_REQUEST_UPDATE = 0x10,
	PLDM_PASS_COMPONENT_TABLE = 0x13,
	PLDM_UPDATE_COMPONENT = 0x14
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

/** @struct query_device_identifiers_resp
 *
 *  Structure representing query device identifiers response.
 */
struct query_device_identifiers_resp {
	uint8_t completion_code;
	uint32_t device_identifiers_len;
	uint8_t descriptor_count;
} __attribute__((packed));

/** @struct get_firmware_parameters_resp
 *
 *  Structure representing get firmware parameters response.
 */
struct get_firmware_parameters_resp {
	uint8_t completion_code;
	bitfield32_t capabilities_during_update;
	uint16_t comp_count;
	uint8_t active_comp_image_set_ver_str_type;
	uint8_t active_comp_image_set_ver_str_len;
	uint8_t pending_comp_image_set_ver_str_type;
	uint8_t pending_comp_image_set_ver_str_len;
} __attribute__((packed));

/** @struct get_firmware_parameters_resp
 *
 *  Structure representing component parameter table entries.
 */
struct component_parameter_table {
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

/* @struct request_update_req
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

/* @struct request_update_resp
 *
 *  Structure representing Request Update response
 */
struct pldm_request_update_resp {
	uint8_t completion_code;
	uint16_t fd_meta_data_len;
	uint8_t fd_pkg_data;
} __attribute__((packed));

/* @struct pass_component_table_req
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

/* @struct pass_component_table_resp
 *
 *  Structure representing Pass Component Table response
 */
struct pldm_pass_component_table_resp {
	uint8_t completion_code;
	uint8_t comp_resp;
	uint8_t comp_resp_code;
} __attribute__((packed));

/* @struct update_component_req
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

/** @brief Decode a GetFirmwareParameters component response
 *
 *  Note:
 *  * If the return value is not PLDM_SUCCESS, it represents a
 * transport layer error.
 *  * If the completion_code value is not PLDM_SUCCESS, it represents a
 * protocol layer error and all the out-parameters are invalid.
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] component_data - Pointer to component parameter table
 *  @param[out] active_comp_ver_str - Pointer to active component version string
 *  @param[out] pending_comp_ver_str - Pointer to pending component version
 * string
 *  @return pldm_completion_codes
 */
int decode_get_firmware_parameters_comp_resp(
    const uint8_t *msg, const size_t payload_length,
    struct component_parameter_table *component_data,
    struct variable_field *active_comp_ver_str,
    struct variable_field *pending_comp_ver_str);

/** @brief Create a PLDM request message for QueryDeviceIdentifiers
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] payload_length - Length of the request message payload
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_query_device_identifiers_req(const uint8_t instance_id,
					const size_t payload_length,
					struct pldm_msg *msg);

/** @brief Decode a QueryDeviceIdentifiers response message
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] device_identifiers_len - Pointer to device identifiers length
 *  @param[out] descriptor_count - Pointer to descriptor count
 *  @param[out] descriptor_data - Pointer to descriptor data
 *  @return pldm_completion_codes
 */
int decode_query_device_identifiers_resp(const struct pldm_msg *msg,
					 const size_t payload_length,
					 uint8_t *completion_code,
					 uint32_t *device_identifiers_len,
					 uint8_t *descriptor_count,
					 uint8_t **descriptor_data);

/** @brief Create a PLDM request message for GetFirmwareParameters
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @param[in] payload_length - Length of the request message payload
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_get_firmware_parameters_req(const uint8_t instance_id,
				       struct pldm_msg *msg,
				       const size_t payload_length);

/** @brief Decode a GetFirmwareParameters component image set response
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] resp_data - Pointer to get firmware parameters response
 *  @param[out] active_comp_image_set_ver_str - Pointer to active component
 * image set version string
 *  @param[out] pending_comp_image_set_ver_str - Pointer to pending component
 * image set version string
 *  @return pldm_completion_codes
 */
int decode_get_firmware_parameters_comp_img_set_resp(
    const struct pldm_msg *msg, const size_t payload_length,
    uint8_t *completion_code, struct get_firmware_parameters_resp *resp_data,
    struct variable_field *active_comp_image_set_ver_str,
    struct variable_field *pending_comp_image_set_ver_str);

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

#ifdef __cplusplus
}
#endif

#endif // End of FW_UPDATE_H
