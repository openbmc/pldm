#ifndef FW_UPDATE_H
#define FW_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif
#include "base.h"
#include "utils.h"

#define PLDM_FWUP_COMPONENT_BITMAP_MULTIPLE 8
#define PLDM_FWUP_INVALID_COMPONENT_COMPARISON_TIMESTAMP 0xFFFFFFFF
#define PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES 0
/** @brief Minimum length of device descriptor, 2 bytes for descriptor type,
 *         2 bytes for descriptor length and atleast 1 byte of descriptor data
 */
#define PLDM_FWUP_DEVICE_DESCRIPTOR_MIN_LEN 5
#define PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES 0
#define PLDM_FWUP_BASELINE_TRANSFER_SIZE 32
#define PLDM_FWUP_MIN_OUTSTANDING_REQ 1

/* Maximum progress percentage value*/
#define FW_UPDATE_MAX_PROGRESS_PERCENT 0x65

/** @brief PLDM Firmware update commands
 */
enum pldm_firmware_update_commands {
	PLDM_QUERY_DEVICE_IDENTIFIERS = 0x01,
	PLDM_GET_FIRMWARE_PARAMETERS = 0x02,
	PLDM_REQUEST_UPDATE = 0x10,
	PLDM_PASS_COMPONENT_TABLE = 0x13,
	PLDM_UPDATE_COMPONENT = 0x14,
	PLDM_GET_STATUS = 0x1B,
	PLDM_CANCEL_UPDATE = 0x1D,
	PLDM_CANCEL_UPDATE_COMPONENT = 0x1C,
	PLDM_ACTIVATE_FIRMWARE = 0x1A,
	PLDM_REQUEST_FIRMWARE_DATA = 0x15,
	PLDM_TRANSFER_COMPLETE = 0x16,
	PLDM_VERIFY_COMPLETE = 0x17,
	PLDM_APPLY_COMPLETE = 0x18
};

/** @brief PLDM Firmware update completion codes
 */
enum pldm_firmware_update_completion_codes {
	PLDM_FWUP_NOT_IN_UPDATE_MODE = 0x80,
	PLDM_FWUP_ALREADY_IN_UPDATE_MODE = 0x81,
	PLDM_FWUP_DATA_OUT_OF_RANGE = 0x82,
	PLDM_FWUP_INVALID_TRANSFER_LENGTH = 0x83,
	PLDM_FWUP_INVALID_STATE_FOR_COMMAND = 0x84,
	PLDM_FWUP_INCOMPLETE_UPDATE = 0x85,
	PLDM_FWUP_BUSY_IN_BACKGROUND = 0x86,
	PLDM_FWUP_CANCEL_PENDING = 0x87,
	PLDM_FWUP_COMMAND_NOT_EXPECTED = 0x87,
	PLDM_FWUP_RETRY_REQUEST_FW_DATA = 0x89,
	PLDM_FWUP_UNABLE_TO_INITIATE_UPDATE = 0x8A,
	PLDM_FWUP_ACTIVATION_NOT_REQUIRED = 0x8B,
	PLDM_FWUP_SELF_CONTAINED_ACTIVATION_NOT_PERMITTED = 0x8C,
	PLDM_FWUP_NO_DEVICE_METADATA = 0x8D,
	PLDM_FWUP_RETRY_REQUEST_UPDATE = 0x8E,
	PLDM_FWUP_NO_PACKAGE_DATA = 0x8F,
	PLDM_FWUP_INVALID_TRANSFER_HANDLE = 0x90,
	PLDM_FWUP_INVALID_TRANSFER_OPERATION_FLAG = 0x91,
	PLDM_FWUP_ACTIVATE_PENDING_IMAGE_NOT_PERMITTED = 0x92,
	PLDM_FWUP_PACKAGE_DATA_ERROR = 0x93
};

/** @brief String type values defined in the PLDM firmware update specification
 */
enum pldm_firmware_update_string_type {
	PLDM_STR_TYPE_UNKNOWN = 0,
	PLDM_STR_TYPE_ASCII = 1,
	PLDM_STR_TYPE_UTF_8 = 2,
	PLDM_STR_TYPE_UTF_16 = 3,
	PLDM_STR_TYPE_UTF_16LE = 4,
	PLDM_STR_TYPE_UTF_16BE = 5
};

/** @brief Descriptor types defined in PLDM firmware update specification
 */
enum pldm_firmware_update_descriptor_types {
	PLDM_FWUP_PCI_VENDOR_ID = 0x0000,
	PLDM_FWUP_IANA_ENTERPRISE_ID = 0x0001,
	PLDM_FWUP_UUID = 0x0002,
	PLDM_FWUP_PNP_VENDOR_ID = 0x0003,
	PLDM_FWUP_ACPI_VENDOR_ID = 0x0004,
	PLDM_FWUP_IEEE_ASSIGNED_COMPANY_ID = 0x0005,
	PLDM_FWUP_SCSI_VENDOR_ID = 0x0006,
	PLDM_FWUP_PCI_DEVICE_ID = 0x0100,
	PLDM_FWUP_PCI_SUBSYSTEM_VENDOR_ID = 0x0101,
	PLDM_FWUP_PCI_SUBSYSTEM_ID = 0x0102,
	PLDM_FWUP_PCI_REVISION_ID = 0x0103,
	PLDM_FWUP_PNP_PRODUCT_IDENTIFIER = 0x0104,
	PLDM_FWUP_ACPI_PRODUCT_IDENTIFIER = 0x0105,
	PLDM_FWUP_ASCII_MODEL_NUMBER_LONG_STRING = 0x0106,
	PLDM_FWUP_ASCII_MODEL_NUMBER_SHORT_STRING = 0x0107,
	PLDM_FWUP_SCSI_PRODUCT_ID = 0x0108,
	PLDM_FWUP_UBM_CONTROLLER_DEVICE_CODE = 0x0109,
	PLDM_FWUP_VENDOR_DEFINED = 0xFFFF
};

/** @brief Descriptor types length defined in PLDM firmware update specification
 */
enum pldm_firmware_update_descriptor_types_length {
	PLDM_FWUP_PCI_VENDOR_ID_LENGTH = 2,
	PLDM_FWUP_IANA_ENTERPRISE_ID_LENGTH = 4,
	PLDM_FWUP_UUID_LENGTH = 16,
	PLDM_FWUP_PNP_VENDOR_ID_LENGTH = 3,
	PLDM_FWUP_ACPI_VENDOR_ID_LENGTH = 4,
	PLDM_FWUP_IEEE_ASSIGNED_COMPANY_ID_LENGTH = 3,
	PLDM_FWUP_SCSI_VENDOR_ID_LENGTH = 8,
	PLDM_FWUP_PCI_DEVICE_ID_LENGTH = 2,
	PLDM_FWUP_PCI_SUBSYSTEM_VENDOR_ID_LENGTH = 2,
	PLDM_FWUP_PCI_SUBSYSTEM_ID_LENGTH = 2,
	PLDM_FWUP_PCI_REVISION_ID_LENGTH = 1,
	PLDM_FWUP_PNP_PRODUCT_IDENTIFIER_LENGTH = 4,
	PLDM_FWUP_ACPI_PRODUCT_IDENTIFIER_LENGTH = 4,
	PLDM_FWUP_ASCII_MODEL_NUMBER_LONG_STRING_LENGTH = 40,
	PLDM_FWUP_ASCII_MODEL_NUMBER_SHORT_STRING_LENGTH = 10,
	PLDM_FWUP_SCSI_PRODUCT_ID_LENGTH = 16,
	PLDM_FWUP_UBM_CONTROLLER_DEVICE_CODE_LENGTH = 4
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
	PLDM_COMP_UNKNOWN = 0x0000,
	PLDM_COMP_OTHER = 0x0001,
	PLDM_COMP_DRIVER = 0x0002,
	PLDM_COMP_CONFIGURATION_SOFTWARE = 0x0003,
	PLDM_COMP_APPLICATION_SOFTWARE = 0x0004,
	PLDM_COMP_INSTRUMENTATION = 0x0005,
	PLDM_COMP_FIRMWARE_OR_BIOS = 0x0006,
	PLDM_COMP_DIAGNOSTIC_SOFTWARE = 0x0007,
	PLDM_COMP_OPERATING_SYSTEM = 0x0008,
	PLDM_COMP_MIDDLEWARE = 0x0009,
	PLDM_COMP_FIRMWARE = 0x000A,
	PLDM_COMP_BIOS_OR_FCODE = 0x000B,
	PLDM_COMP_SUPPORT_OR_SERVICEPACK = 0x000C,
	PLDM_COMP_SOFTWARE_BUNDLE = 0x000D,
	PLDM_COMP_DOWNSTREAM_DEVICE = 0xffff
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

/** @brief PLDM FWU codes for Self Contained Activation Request
 */
enum self_contained_activation_req {
	NOT_CONTAINING_SELF_ACTIVATED_COMPONENTS = 0,
	CONTAINS_SELF_ACTIVATED_COMPONENTS = 1
};

/** @brief PLDM FWU codes for transfer result
 */
enum pldm_firmware_update_transfer_result {
	PLDM_FWUP_TRASFER_SUCCESS = 0x00,
	PLDM_FWUP_TRANSFER_COMPLETE_WITH_ERROR = 0x02,
	PLDM_FWUP_FD_ABORTED_TRANSFER = 0x03,
	PLDM_FWUP_VENDOR_TRANSFER_RESULT_RANGE_MIN = 0x70,
	PLDM_FWUP_VENDOR_TRANSFER_RESULT_RANGE_MAX = 0x8F
};

/**@brief PLDM FWU common error codes for transfer result, verify result and
 * apply result
 */
enum pldm_firmware_update_common_error_code {
	PLDM_FWUP_TIME_OUT = 0x09,
	PLDM_FWUP_GENERIC_ERROR = 0x0A
};

/**@brief PLDM FWU codes for verify result
 */
enum pldm_fwu_verify_result {
	PLDM_FWUP_VERIFY_SUCCESS = 0x00,
	PLDM_FWUP_VERIFY_COMPLETED_WITH_FAILURE = 0x01,
	PLDM_FWUP_VERIFY_COMPLETED_WITH_ERROR = 0x02,
	PLDM_FWUP_VENDOR_SPEC_STATUS_RANGE_MIN = 0x90,
	PLDM_FWUP_VENDOR_SPEC_STATUS_RANGE_MAX = 0xAF
};

/**@brief PLDM FWU result of the apply result
 */
enum pldm_fwu_apply_result {
	PLDM_FWUP_APPLY_SUCCESS = 0x00,
	PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD = 0x01,
	PLDM_FWUP_APPLY_COMPLETED_WITH_FAILURE = 0x02,
	PLDM_FWUP_VENDOR_APPLY_RESULT_RANGE_MIN = 0xB0,
	PLDM_FWUP_VENDOR_APPLY_RESULT_RANGE_MAX = 0xCF
};
/** @brief PLDM FWU values for Component Activation Methods Modification
 */
enum comp_activation_methods_modification {
	APPLY_AUTOMATIC = 0,
	APPLY_SELF_CONTAINED = 1,
	APPLY_MEDIUM_SPECIFIC_RESET = 2,
	APPLY_SYSTEM_REBOOT = 3,
	APPLY_DC_POWER_CYCLE = 4,
	APPLY_AC_POWER_CYCLE = 5
};

/** @struct pldm_package_header_information
 *
 *  Structure representing fixed part of package header information
 */
struct pldm_package_header_information {
	uint8_t uuid[PLDM_FWUP_UUID_LENGTH];
	uint8_t package_header_format_version;
	uint16_t package_header_size;
	uint8_t timestamp104[PLDM_TIMESTAMP104_SIZE];
	uint16_t component_bitmap_bit_length;
	uint8_t package_version_string_type;
	uint8_t package_version_string_length;
} __attribute__((packed));

/** @struct pldm_firmware_device_id_record
 *
 *  Structure representing firmware device ID record
 */
struct pldm_firmware_device_id_record {
	uint16_t record_length;
	uint8_t descriptor_count;
	bitfield32_t device_update_option_flags;
	uint8_t comp_image_set_version_string_type;
	uint8_t comp_image_set_version_string_length;
	uint16_t fw_device_pkg_data_length;
} __attribute__((packed));

/** @struct pldm_descriptor_tlv
 *
 *  Structure representing descriptor type, length and value
 */
struct pldm_descriptor_tlv {
	uint16_t descriptor_type;
	uint16_t descriptor_length;
	uint8_t descriptor_data[1];
} __attribute__((packed));

/** @struct pldm_vendor_defined_descriptor_title_data
 *
 *  Structure representing vendor defined descriptor title sections
 */
struct pldm_vendor_defined_descriptor_title_data {
	uint8_t vendor_defined_descriptor_title_str_type;
	uint8_t vendor_defined_descriptor_title_str_len;
	uint8_t vendor_defined_descriptor_title_str[1];
} __attribute__((packed));

/** @struct pldm_component_image_information
 *
 *  Structure representing fixed part of individual component information in
 *  PLDM firmware update package
 */
struct pldm_component_image_information {
	uint16_t comp_classification;
	uint16_t comp_identifier;
	uint32_t comp_comparison_stamp;
	bitfield16_t comp_options;
	bitfield16_t requested_comp_activation_method;
	uint32_t comp_location_offset;
	uint32_t comp_size;
	uint8_t comp_version_string_type;
	uint8_t comp_version_string_length;
} __attribute__((packed));

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
 *  Structure representing the fixed part of GetFirmwareParameters response
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

/** @struct pldm_request_update_req
 *
 *  Structure representing fixed part of Request Update request
 */
struct pldm_request_update_req {
	uint32_t max_transfer_size;
	uint16_t num_of_comp;
	uint8_t max_outstanding_transfer_req;
	uint16_t pkg_data_len;
	uint8_t comp_image_set_ver_str_type;
	uint8_t comp_image_set_ver_str_len;
} __attribute__((packed));

/** @struct pldm_request_update_resp
 *
 *  Structure representing Request Update response
 */
struct pldm_request_update_resp {
	uint8_t completion_code;
	uint16_t fd_meta_data_len;
	uint8_t fd_will_send_pkg_data;
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

/** @struct activate_firmware_req
 *
 *  Structure representing Activate Firmware request
 */
struct pldm_activate_firmware_req {
	bool8_t self_contained_activation_req;
} __attribute__((packed));

/** @struct activate_firmware_resp
 *
 *  Structure representing Activate Firmware response
 */
struct pldm_activate_firmware_resp {
	uint8_t completion_code;
	uint16_t estimated_time_activation;
} __attribute__((packed));

/** @struct request_firmware_data_req
 *
 *  Structure representing Request firmware data request.
 */
struct request_firmware_data_req {
	uint32_t offset;
	uint32_t length;
} __attribute__((packed));

/** @struct apply_complete_req
 *
 *  Structure representing Apply Complete request.
 */
struct apply_complete_req {
	uint8_t apply_result;
	bitfield16_t comp_activation_methods_modification;
} __attribute__((packed));

/** @brief Decode the PLDM package header information
 *
 *  @param[in] data - pointer to package header information
 *  @param[in] length - available length in the firmware update package
 *  @param[out] package_header_info - pointer to fixed part of PLDM package
 *                                    header information
 *  @param[out] package_version_str - pointer to package version string
 *
 *  @return pldm_completion_codes
 */
int decode_pldm_package_header_info(
    const uint8_t *data, size_t length,
    struct pldm_package_header_information *package_header_info,
    struct variable_field *package_version_str);

/** @brief Decode individual firmware device ID record
 *
 *  @param[in] data - pointer to firmware device ID record
 *  @param[in] length - available length in the firmware update package
 *  @param[in] component_bitmap_bit_length - ComponentBitmapBitLengthfield
 *                                           parsed from the package header info
 *  @param[out] fw_device_id_record - pointer to fixed part of firmware device
 *                                    id record
 *  @param[out] applicable_components - pointer to ApplicableComponents
 *  @param[out] comp_image_set_version_str - pointer to
 *                                           ComponentImageSetVersionString
 *  @param[out] record_descriptors - pointer to RecordDescriptors
 *  @param[out] fw_device_pkg_data - pointer to FirmwareDevicePackageData
 *
 *  @return pldm_completion_codes
 */
int decode_firmware_device_id_record(
    const uint8_t *data, size_t length, uint16_t component_bitmap_bit_length,
    struct pldm_firmware_device_id_record *fw_device_id_record,
    struct variable_field *applicable_components,
    struct variable_field *comp_image_set_version_str,
    struct variable_field *record_descriptors,
    struct variable_field *fw_device_pkg_data);

/** @brief Decode the record descriptor entries in the firmware update package
 *         and the Descriptors in the QueryDeviceIDentifiers command
 *
 *  @param[in] data - pointer to descriptor entry
 *  @param[in] length - remaining length of the descriptor data
 *  @param[out] descriptor_type - pointer to descriptor type
 *  @param[out] descriptor_data - pointer to descriptor data
 *
 *  @return pldm_completion_codes
 */
int decode_descriptor_type_length_value(const uint8_t *data, size_t length,
					uint16_t *descriptor_type,
					struct variable_field *descriptor_data);

/** @brief Decode the vendor defined descriptor value
 *
 *  @param[in] data - pointer to vendor defined descriptor value
 *  @param[in] length - length of the vendor defined descriptor value
 *  @param[out] descriptor_title_str_type - pointer to vendor defined descriptor
 *                                          title string type
 *  @param[out] descriptor_title_str - pointer to vendor defined descriptor
 *                                     title string
 *  @param[out] descriptor_data - pointer to vendor defined descriptor data
 *
 *  @return pldm_completion_codes
 */
int decode_vendor_defined_descriptor_value(
    const uint8_t *data, size_t length, uint8_t *descriptor_title_str_type,
    struct variable_field *descriptor_title_str,
    struct variable_field *descriptor_data);

/** @brief Decode individual component image information
 *
 *  @param[in] data - pointer to component image information
 *  @param[in] length - available length in the firmware update package
 *  @param[out] pldm_comp_image_info - pointer to fixed part of component image
 *                                     information
 *  @param[out] comp_version_str - pointer to component version string
 *
 *  @return pldm_completion_codes
 */
int decode_pldm_comp_image_info(
    const uint8_t *data, size_t length,
    struct pldm_component_image_information *pldm_comp_image_info,
    struct variable_field *comp_version_str);

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

/** @brief Decode GetFirmwareParameters response
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] resp_data - Pointer to get firmware parameters response
 *  @param[out] active_comp_image_set_ver_str - Pointer to active component
 *                                              image set version string
 *  @param[out] pending_comp_image_set_ver_str - Pointer to pending component
 *                                               image set version string
 *  @param[out] comp_parameter_table - Pointer to component parameter table
 *
 *  @return pldm_completion_codes
 */
int decode_get_firmware_parameters_resp(
    const struct pldm_msg *msg, size_t payload_length,
    struct pldm_get_firmware_parameters_resp *resp_data,
    struct variable_field *active_comp_image_set_ver_str,
    struct variable_field *pending_comp_image_set_ver_str,
    struct variable_field *comp_parameter_table);

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

/** @brief Create PLDM request message for RequestUpdate
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] max_transfer_size - Maximum size of the variable payload allowed
 *                                 to be requested via RequestFirmwareData
 *                                 command
 *  @param[in] num_of_comp - Total number of components that will be passed to
 *                           the FD during the update
 *  @param[in] max_outstanding_transfer_req - Total number of outstanding
 * 											  RequestFirmwareData
 * commands that can be sent by the FD
 *  @param[in] pkg_data_len - Value of the FirmwareDevicePackageDataLength field
 *                            present in firmware package header
 *  @param[in] comp_image_set_ver_str_type - StringType of
 *                                           ComponentImageSetVersionString
 *  @param[in] comp_image_set_ver_str_len - The length of the
 *                                          ComponentImageSetVersionString
 *  @param[in] comp_img_set_ver_str - Component Image Set version information
 *  @param[in,out] msg - Message will be written to this
 *  @param[in] payload_length - Length of request message payload
 *
 *  @return pldm_completion_codes
 *
 *  @note Caller is responsible for memory alloc and dealloc of param
 *        'msg.payload'
 */
int encode_request_update_req(uint8_t instance_id, uint32_t max_transfer_size,
			      uint16_t num_of_comp,
			      uint8_t max_outstanding_transfer_req,
			      uint16_t pkg_data_len,
			      uint8_t comp_image_set_ver_str_type,
			      uint8_t comp_image_set_ver_str_len,
			      const struct variable_field *comp_img_set_ver_str,
			      struct pldm_msg *msg, size_t payload_length);

/** @brief Decode a RequestUpdate response message
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to hold the completion code
 *  @param[out] fd_meta_data_len - Pointer to hold the length of FD metadata
 *  @param[out] fd_will_send_pkg_data - Pointer to hold information whether FD
 *                                      will send GetPackageData command
 *  @return pldm_completion_codes
 */
int decode_request_update_resp(const struct pldm_msg *msg,
			       size_t payload_length, uint8_t *completion_code,
			       uint16_t *fd_meta_data_len,
			       uint8_t *fd_will_send_pkg_data);

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
    uint8_t instance_id, struct pldm_msg *msg, size_t payload_length,
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

/** @brief Create a PLDM request message for CancelUpdateComponent
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_cancel_update_component_req(const uint8_t instance_id,
				       struct pldm_msg *msg);

/** @brief Decode CancelUpdateComponent response message
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @return pldm_completion_codes
 */
int decode_cancel_update_component_resp(const struct pldm_msg *msg,
					const size_t payload_length,
					uint8_t *completion_code);

/** @brief Create a PLDM request message for ActivateFirmware
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message will be written to this
 *  @param[in] payload_length - Length of request message payload
 *  @param[in] self_contained_activation_req returns True if FD shall activate
 * all self-contained components and returns False if FD shall not activate any
 * self-contained components.
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.payload'
 */
int encode_activate_firmware_req(const uint8_t instance_id,
				 struct pldm_msg *msg,
				 const size_t payload_length,
				 const bool8_t self_contained_activation_req);

/** @brief Decode a ActivateFirmware response message
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of response message payload
 *  @param[out] completion_code - Pointer to response msg's PLDM completion code
 *  @param[out] estimated_time_activation - Pointer to Estimated Time For Self
 * Contained Activation request firmware data information
 *  @return pldm_completion_codes
 */
int decode_activate_firmware_resp(const struct pldm_msg *msg,
				  const size_t payload_length,
				  uint8_t *completion_code,
				  uint16_t *estimated_time_activation);

/** @brief Decode a RequestFirmwareData request message
 *
 *	@param[in] msg - request message
 *	@param[in] payload_length - Length of request message payload
 *	@param[out] offset - pointer to offset of the component image segment
 *	@param[out] length - pointer to size of the component image segment
 * requested by the FD information
 *	@return pldm_completion_codes
 */
int decode_request_firmware_data_req(const struct pldm_msg *msg,
				     const size_t payload_length,
				     uint32_t *offset, uint32_t *length);

/** @brief Create a PLDM response message for RequestFirmwareData
 *
 *	@param[in] instance_id - Message's instance id
 *	@param[in,out] msg - Message will be written to this
 *  @param[in] payload_length - Length of response message payload
 *	@param[in] completion_code - Pointer to response msg's PLDM completion
 *code
 *	@param[in] component_image_portion - Pointer which holds image segment
 * information
 *	@return pldm_completion_codes
 *	@note  Caller is responsible for memory alloc and dealloc of param
 *		   'msg.payload'
 */
int encode_request_firmware_data_resp(
    const uint8_t instance_id, struct pldm_msg *msg,
    const size_t payload_length, const uint8_t completion_code,
    struct variable_field *component_image_portion);

/** @brief Decode TransferComplete request message
 *
 *  @param[in] msg - request message
 *  @param[out] transfer_result - Pointer to transfer result
 *  @return pldm_completion_codes
 */
int decode_transfer_complete_req(const struct pldm_msg *msg,
				 uint8_t *transfer_result);

/** @brief Create a PLDM response message for TransferComplete
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - Pointer to response msg's PLDM completion code
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 * 'msg.payload'
 */
int encode_transfer_complete_resp(const uint8_t instance_id,
				  const uint8_t completion_code,
				  struct pldm_msg *msg);

/** @brief Decode VerifyComplete request message
 *
 *  @param[in] msg - Response message
 *  @param[in] verify_result - pointer to verify result from FD
 *  @return pldm_completion_codes
 */
int decode_verify_complete_req(const struct pldm_msg *msg,
			       uint8_t *verify_result);

/** @brief Create a PLDM response message for VerifyComplete
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - Pointer to response msg's PLDM completion code
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 * 'msg.payload'
 */
int encode_verify_complete_resp(const uint8_t instance_id,
				const uint8_t completion_code,
				struct pldm_msg *msg);

/** @brief Decode ApplyComplete request message
 *
 *  @param[in] msg - Response message
 *  @param[in] payload_length - Length of request message payload
 *  @param[in] apply_result - pointer to ApplyResult from FD
 *  @param[in] comp_activation_methods_modification - pointer to Component
 * Activation Methods Modification
 *  @return pldm_completion_codes
 */
int decode_apply_complete_req(
    const struct pldm_msg *msg, const size_t payload_length,
    uint8_t *apply_result, bitfield16_t *comp_activation_methods_modification);
	
/** @brief Create a PLDM response message for ApplyComplete
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - completion code
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 * 'msg.payload'
 */
int encode_apply_complete_resp(const uint8_t instance_id,
			       const uint8_t completion_code,
			       struct pldm_msg *msg);
#ifdef __cplusplus
}
#endif

#endif // End of FW_UPDATE_H
