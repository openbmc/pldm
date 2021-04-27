#include "firmware_update.h"
#include <endian.h>
#include <string.h>

int encode_query_device_identifiers_req(uint8_t instance_id,
					size_t payload_length,
					struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	return encode_pldm_header_only(PLDM_REQUEST, instance_id, PLDM_FWUP,
				       PLDM_QUERY_DEVICE_IDENTIFIERS, msg);
}

int decode_query_device_identifiers_resp(const struct pldm_msg *msg,
					 size_t payload_length,
					 uint8_t *completion_code,
					 uint32_t *device_identifiers_len,
					 uint8_t *descriptor_count,
					 uint8_t **descriptor_data)
{
	if (msg == NULL || completion_code == NULL ||
	    device_identifiers_len == NULL || descriptor_count == NULL ||
	    descriptor_data == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg->payload[0];
	if (PLDM_SUCCESS != *completion_code) {
		return PLDM_SUCCESS;
	}

	if (payload_length <
	    sizeof(struct pldm_query_device_identifiers_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_query_device_identifiers_resp *response =
	    (struct pldm_query_device_identifiers_resp *)msg->payload;
	*device_identifiers_len = le32toh(response->device_identifiers_len);

	if (*device_identifiers_len < PLDM_FWUP_DEVICE_DESCRIPTOR_MIN_LEN) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	if (payload_length !=
	    sizeof(struct pldm_query_device_identifiers_resp) +
		*device_identifiers_len) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	*descriptor_count = response->descriptor_count;

	if (*descriptor_count == 0) {
		return PLDM_ERROR_INVALID_DATA;
	}
	*descriptor_data =
	    (uint8_t *)(msg->payload +
			sizeof(struct pldm_query_device_identifiers_resp));
	return PLDM_SUCCESS;
}

int encode_get_firmware_parameters_req(uint8_t instance_id,
				       size_t payload_length,
				       struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	return encode_pldm_header_only(PLDM_REQUEST, instance_id, PLDM_FWUP,
				       PLDM_GET_FIRMWARE_PARAMETERS, msg);
}

int decode_get_firmware_parameters_resp_comp_set_info(
    const struct pldm_msg *msg, size_t payload_length,
    struct pldm_get_firmware_parameters_resp *resp_data,
    struct variable_field *active_comp_image_set_ver_str,
    struct variable_field *pending_comp_image_set_ver_str)
{
	if (msg == NULL || resp_data == NULL ||
	    active_comp_image_set_ver_str == NULL ||
	    pending_comp_image_set_ver_str == NULL) {

		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length < sizeof(struct pldm_get_firmware_parameters_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_get_firmware_parameters_resp *response =
	    (struct pldm_get_firmware_parameters_resp *)msg->payload;

	resp_data->completion_code = response->completion_code;

	if (PLDM_SUCCESS != resp_data->completion_code) {
		return PLDM_SUCCESS;
	}

	if (response->active_comp_image_set_ver_str_len == 0) {
		return PLDM_ERROR_INVALID_DATA;
	}

	size_t resp_len = sizeof(struct pldm_get_firmware_parameters_resp) +
			  response->active_comp_image_set_ver_str_len +
			  response->pending_comp_image_set_ver_str_len;

	if (payload_length != resp_len) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	resp_data->capabilities_during_update.value =
	    le32toh(response->capabilities_during_update.value);

	resp_data->comp_count = le16toh(response->comp_count);
	if (resp_data->comp_count == 0) {
		return PLDM_ERROR;
	}

	resp_data->active_comp_image_set_ver_str_type =
	    response->active_comp_image_set_ver_str_type;
	resp_data->active_comp_image_set_ver_str_len =
	    response->active_comp_image_set_ver_str_len;
	resp_data->pending_comp_image_set_ver_str_type =
	    response->pending_comp_image_set_ver_str_type;
	resp_data->pending_comp_image_set_ver_str_len =
	    response->pending_comp_image_set_ver_str_len;

	active_comp_image_set_ver_str->ptr =
	    msg->payload + sizeof(struct pldm_get_firmware_parameters_resp);
	active_comp_image_set_ver_str->length =
	    resp_data->active_comp_image_set_ver_str_len;

	if (resp_data->pending_comp_image_set_ver_str_len != 0) {
		pending_comp_image_set_ver_str->ptr =
		    msg->payload +
		    sizeof(struct pldm_get_firmware_parameters_resp) +
		    resp_data->active_comp_image_set_ver_str_len;
		pending_comp_image_set_ver_str->length =
		    resp_data->pending_comp_image_set_ver_str_len;
	} else {
		pending_comp_image_set_ver_str->ptr = NULL;
		pending_comp_image_set_ver_str->length = 0;
	}

	return PLDM_SUCCESS;
}

int decode_get_firmware_parameters_resp_comp_entry(
    const uint8_t *data, size_t length,
    struct pldm_component_parameter_entry *component_data,
    struct variable_field *active_comp_ver_str,
    struct variable_field *pending_comp_ver_str)
{
	if (data == NULL || component_data == NULL ||
	    active_comp_ver_str == NULL || pending_comp_ver_str == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (length < sizeof(struct pldm_component_parameter_entry)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_component_parameter_entry *entry =
	    (struct pldm_component_parameter_entry *)(data);
	if (entry->active_comp_ver_str_len == 0) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	size_t entry_length = sizeof(struct pldm_component_parameter_entry) +
			      entry->active_comp_ver_str_len +
			      entry->pending_comp_ver_str_len;

	if (length != entry_length) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	component_data->comp_classification =
	    le16toh(entry->comp_classification);
	component_data->comp_identifier = le16toh(entry->comp_identifier);
	component_data->comp_classification_index =
	    entry->comp_classification_index;
	component_data->active_comp_comparison_stamp =
	    le32toh(entry->active_comp_comparison_stamp);
	component_data->active_comp_ver_str_type =
	    entry->active_comp_ver_str_type;
	component_data->active_comp_ver_str_len =
	    entry->active_comp_ver_str_len;
	memcpy(component_data->active_comp_release_date,
	       entry->active_comp_release_date,
	       sizeof(entry->active_comp_release_date));
	component_data->pending_comp_comparison_stamp =
	    le32toh(entry->pending_comp_comparison_stamp);
	component_data->pending_comp_ver_str_type =
	    entry->pending_comp_ver_str_type;
	component_data->pending_comp_ver_str_len =
	    entry->pending_comp_ver_str_len;
	memcpy(component_data->pending_comp_release_date,
	       entry->pending_comp_release_date,
	       sizeof(entry->pending_comp_release_date));
	component_data->comp_activation_methods.value =
	    le16toh(entry->comp_activation_methods.value);
	component_data->capabilities_during_update.value =
	    le32toh(entry->capabilities_during_update.value);

	active_comp_ver_str->ptr =
	    data + sizeof(struct pldm_component_parameter_entry);
	active_comp_ver_str->length = entry->active_comp_ver_str_len;

	if (entry->pending_comp_ver_str_len != 0) {

		pending_comp_ver_str->ptr =
		    data + sizeof(struct pldm_component_parameter_entry) +
		    entry->active_comp_ver_str_len;
		pending_comp_ver_str->length = entry->pending_comp_ver_str_len;
	} else {
		pending_comp_ver_str->ptr = NULL;
		pending_comp_ver_str->length = 0;
	}
	return PLDM_SUCCESS;
}

/** @brief Check whether Component Version String Type or Component Image Set
 * Version String Type is valid
 *
 *  @return true if is from below mentioned values, false if not
 */
static bool check_comp_ver_str_type_valid(const uint8_t comp_type)
{
	switch (comp_type) {
	case PLDM_COMP_VER_STR_TYPE_UNKNOWN:
	case PLDM_COMP_ASCII:
	case PLDM_COMP_UTF_8:
	case PLDM_COMP_UTF_16:
	case PLDM_COMP_UTF_16LE:
	case PLDM_COMP_UTF_16BE:
		return true;

	default:
		return false;
	}
}

/** @brief Check whether Request Update Request is valid
 *
 *  @return true if request is valid,false if not
 */
static bool
validate_request_update_req(const struct pldm_request_update_req *data)
{
	if (data->max_transfer_size < PLDM_FWU_BASELINE_TRANSFER_SIZE) {
		return false;
	}
	if (data->max_outstand_transfer_req < PLDM_MIN_OUTSTANDING_REQ) {
		return false;
	}
	if (!check_comp_ver_str_type_valid(data->comp_image_set_ver_str_type)) {
		return false;
	}
	return true;
}

/** @brief Fill the Request Update Request
 *
 */
static void fill_request_update_req(const struct pldm_request_update_req *data,
				    struct pldm_request_update_req *request)
{
	request->max_transfer_size = htole32(data->max_transfer_size);
	request->no_of_comp = htole16(data->no_of_comp);
	request->max_outstand_transfer_req = data->max_outstand_transfer_req;
	request->pkg_data_len = htole16(data->pkg_data_len);
	request->comp_image_set_ver_str_type =
	    data->comp_image_set_ver_str_type;
	request->comp_image_set_ver_str_len = data->comp_image_set_ver_str_len;
}

int encode_request_update_req(const uint8_t instance_id, struct pldm_msg *msg,
			      const size_t payload_length,
			      const struct pldm_request_update_req *data,
			      struct variable_field *comp_img_set_ver_str)
{
	if (msg == NULL || data == NULL || comp_img_set_ver_str == NULL ||
	    comp_img_set_ver_str->ptr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length < sizeof(struct pldm_request_update_req)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	if (payload_length != sizeof(struct pldm_request_update_req) +
				  comp_img_set_ver_str->length) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	if (data->comp_image_set_ver_str_len != comp_img_set_ver_str->length) {
		return PLDM_ERROR_INVALID_DATA;
	}
	int rc = encode_pldm_header_only(PLDM_REQUEST, instance_id, PLDM_FWUP,
					 PLDM_REQUEST_UPDATE, msg);

	if (PLDM_SUCCESS != rc) {
		return rc;
	}

	struct pldm_request_update_req *request =
	    (struct pldm_request_update_req *)msg->payload;

	if (request == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (!validate_request_update_req(data)) {
		return PLDM_ERROR_INVALID_DATA;
	}

	fill_request_update_req(data, request);

	memcpy(msg->payload + sizeof(struct pldm_request_update_req),
	       comp_img_set_ver_str->ptr, comp_img_set_ver_str->length);

	return PLDM_SUCCESS;
}

int decode_request_update_resp(const struct pldm_msg *msg,
			       const size_t payload_length,
			       uint8_t *completion_code,
			       uint16_t *fd_meta_data_len, uint8_t *fd_pkg_data)
{
	if (msg == NULL || completion_code == NULL ||
	    fd_meta_data_len == NULL || fd_pkg_data == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	if (payload_length != sizeof(struct pldm_request_update_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*completion_code = msg->payload[0];

	if (*completion_code != PLDM_SUCCESS) {
		return PLDM_SUCCESS;
	}

	struct pldm_request_update_resp *response =
	    (struct pldm_request_update_resp *)msg->payload;

	if (response == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*fd_meta_data_len = le16toh(response->fd_meta_data_len);

	*fd_pkg_data = response->fd_pkg_data;

	return PLDM_SUCCESS;
}

/** @brief Check whether Component Response Code is valid
 *
 *  @return true if is from below mentioned values, false if not
 */
static bool check_resp_code_valid(const uint8_t comp_resp_code)
{
	switch (comp_resp_code) {
	case COMP_CAN_BE_UPDATED:
	case COMP_COMPARISON_STAMP_IDENTICAL:
	case COMP_COMPARISON_STAMP_LOWER:
	case INVALID_COMP_COMPARISON_STAMP:
	case COMP_CONFLICT:
	case COMP_PREREQUISITES:
	case COMP_NOT_SUPPORTED:
	case COMP_SECURITY_RESTRICTIONS:
	case INCOMPLETE_COMP_IMAGE_SET:
	case COMP_VER_STR_IDENTICAL:
	case COMP_VER_STR_LOWER:
	case FD_DOWN_STREAM_DEVICE_NOT_UPDATE_SUBSEQUENTLY:
		return true;

	default:
		if (comp_resp_code >= FD_VENDOR_COMP_STATUS_CODE_RANGE_MIN &&
		    comp_resp_code <= FD_VENDOR_COMP_STATUS_CODE_RANGE_MAX) {
			return true;
		}
		return false;
	}
}

/** @brief Check whether Component Response is valid
 *
 *  @return true if is from below mentioned values, false if not
 */
static bool check_comp_resp_valid(const uint8_t comp_resp)
{
	switch (comp_resp) {
	case PLDM_COMP_CAN_BE_UPDATEABLE:
	case PLDM_COMP_MAY_BE_UPDATEABLE:
		return true;

	default:
		return false;
	}
}

/** @brief Check whether Component Classification is valid
 *
 *  @return true if it is from below mentioned values, false if not
 */
static bool check_comp_classification_valid(const uint16_t comp_classification)
{
	switch (comp_classification) {
	case COMP_UNKNOWN:
	case COMP_OTHER:
	case COMP_DRIVER:
	case COMP_CONFIGURATION_SOFTWARE:
	case COMP_APPLICATION_SOFTWARE:
	case COMP_INSTRUMENTATION:
	case COMP_FIRMWARE_OR_BIOS:
	case COMP_DIAGNOSTIC_SOFTWARE:
	case COMP_OPERATING_SYSTEM:
	case COMP_MIDDLEWARE:
	case COMP_FIRMWARE:
	case COMP_BIOS_OR_FCODE:
	case COMP_SUPPORT_OR_SERVICEPACK:
	case COMP_SOFTWARE_BUNDLE:
	case COMP_DOWNSTREAM_DEVICE:
		return true;

	default:
		return false;
	}
}

/** @brief Check whether Request Update Request is valid
 *
 *  @return true if request is valid,false if not
 */
static int validate_pass_component_table_req(
    const struct pldm_pass_component_table_req *data)
{
	if (!check_comp_classification_valid(
		htole16(data->comp_classification))) {
		return PLDM_ERROR_INVALID_DATA;
	}
	if (!check_transfer_flag_valid(data->transfer_flag)) {
		return PLDM_INVALID_TRANSFER_OPERATION_FLAG;
	}
	if (!check_comp_ver_str_type_valid(data->comp_ver_str_type)) {
		return PLDM_ERROR_INVALID_DATA;
	}
	return PLDM_SUCCESS;
}

/** @brief Fill the Pass Component Table Request
 *
 */
static void
fill_pass_component_table_req(const struct pldm_pass_component_table_req *data,
			      struct pldm_pass_component_table_req *request)
{
	request->transfer_flag = data->transfer_flag;
	request->comp_classification = htole16(data->comp_classification);
	request->comp_identifier = htole16(data->comp_identifier);
	request->comp_classification_index = data->comp_classification_index;
	request->comp_comparison_stamp = htole32(data->comp_comparison_stamp);
	request->comp_ver_str_type = data->comp_ver_str_type;
	request->comp_ver_str_len = data->comp_ver_str_len;
}

int encode_pass_component_table_req(
    const uint8_t instance_id, struct pldm_msg *msg,
    const size_t payload_length,
    const struct pldm_pass_component_table_req *data,
    struct variable_field *comp_ver_str)
{
	if (msg == NULL || data == NULL || comp_ver_str == NULL ||
	    comp_ver_str->ptr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != sizeof(struct pldm_pass_component_table_req) +
				  comp_ver_str->length) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	if (comp_ver_str->length != data->comp_ver_str_len) {
		return PLDM_ERROR_INVALID_DATA;
	}
	int rc = validate_pass_component_table_req(data);
	if ((PLDM_SUCCESS != rc)) {
		return rc;
	}

	rc = encode_pldm_header_only(PLDM_REQUEST, instance_id, PLDM_FWUP,
				     PLDM_PASS_COMPONENT_TABLE, msg);

	if (PLDM_SUCCESS != rc) {
		return rc;
	}

	struct pldm_pass_component_table_req *request =
	    (struct pldm_pass_component_table_req *)msg->payload;

	if (request == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	fill_pass_component_table_req(data, request);

	memcpy(msg->payload + sizeof(struct pldm_pass_component_table_req),
	       comp_ver_str->ptr, comp_ver_str->length);

	return PLDM_SUCCESS;
}

int decode_pass_component_table_resp(const struct pldm_msg *msg,
				     const size_t payload_length,
				     uint8_t *completion_code,
				     uint8_t *comp_resp,
				     uint8_t *comp_resp_code)
{
	if (msg == NULL || completion_code == NULL || comp_resp == NULL ||
	    comp_resp_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != sizeof(struct pldm_pass_component_table_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*completion_code = msg->payload[0];

	if (*completion_code != PLDM_SUCCESS) {
		return PLDM_SUCCESS;
	}

	struct pldm_pass_component_table_resp *response =
	    (struct pldm_pass_component_table_resp *)msg->payload;

	if (response == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (!check_comp_resp_valid(response->comp_resp)) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*comp_resp = response->comp_resp;

	if (!check_resp_code_valid(response->comp_resp_code)) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*comp_resp_code = response->comp_resp_code;

	return PLDM_SUCCESS;
}

/** @brief Fill the update component Request
 *
 */
static void
fill_update_component_req(const struct pldm_update_component_req *data,
			  struct pldm_update_component_req *request)
{
	request->comp_classification = htole16(data->comp_classification);
	request->comp_identifier = htole16(data->comp_identifier);
	request->comp_classification_index = data->comp_classification_index;
	request->comp_comparison_stamp = htole32(data->comp_comparison_stamp);
	request->comp_image_size = htole32(data->comp_image_size);
	request->update_option_flags.value =
	    htole32(data->update_option_flags.value);
	request->comp_ver_str_type = data->comp_ver_str_type;
	request->comp_ver_str_len = data->comp_ver_str_len;
}
int encode_update_component_req(const uint8_t instance_id, struct pldm_msg *msg,
				const size_t payload_length,
				const struct pldm_update_component_req *data,
				struct variable_field *comp_ver_str)
{
	if (msg == NULL || data == NULL || comp_ver_str == NULL ||
	    comp_ver_str->ptr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length !=
	    sizeof(struct pldm_update_component_req) + comp_ver_str->length) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	if (!check_comp_ver_str_type_valid(data->comp_ver_str_type)) {
		return PLDM_ERROR_INVALID_DATA;
	}
	if (!check_comp_classification_valid(
		htole16(data->comp_classification))) {
		return PLDM_ERROR_INVALID_DATA;
	}
	int rc = encode_pldm_header_only(PLDM_REQUEST, instance_id, PLDM_FWUP,
					 PLDM_UPDATE_COMPONENT, msg);
	if (PLDM_SUCCESS != rc) {
		return rc;
	}

	struct pldm_update_component_req *request =
	    (struct pldm_update_component_req *)msg->payload;

	if (request == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	fill_update_component_req(data, request);
	memcpy(msg->payload + sizeof(struct pldm_update_component_req),
	       comp_ver_str->ptr, comp_ver_str->length);

	return PLDM_SUCCESS;
}

/** @brief Check whether component compatibility response code is valid
 *
 *  @return true if is from below mentioned values, false if not
 */
static bool
check_compatability_resp_code_valid(const uint8_t comp_compatability_resp_code)
{
	switch (comp_compatability_resp_code) {
	case NO_RESPONSE_CODE:
	case COMP_COMPARISON_STAMP_IDENTICAL:
	case COMP_COMPARISON_STAMP_LOWER:
	case INVALID_COMP_COMPARISON_STAMP:
	case COMP_CONFLICT:
	case COMP_PREREQUISITES:
	case COMP_NOT_SUPPORTED:
	case COMP_SECURITY_RESTRICTIONS:
	case INCOMPLETE_COMP_IMAGE_SET:
	case COMPATABILITY_NO_MATCH:
	case COMP_VER_STR_IDENTICAL:
	case COMP_VER_STR_LOWER:
		return true;

	default:
		if (comp_compatability_resp_code >=
			FD_VENDOR_COMP_STATUS_CODE_RANGE_MIN &&
		    comp_compatability_resp_code <=
			FD_VENDOR_COMP_STATUS_CODE_RANGE_MAX) {
			return true;
		}
		return false;
	}
}

/** @brief Check whether component compatibility response is valid
 *
 *  @return true if is from below mentioned values, false if not
 */
static bool
check_compatability_resp_valid(const uint8_t comp_compatability_resp)
{
	switch (comp_compatability_resp) {
	case COMPONENT_CAN_BE_UPDATED:
	case COMPONENT_CANNOT_BE_UPDATED:
		return true;

	default:
		return false;
	}
}

int decode_update_component_resp(const struct pldm_msg *msg,
				 const size_t payload_length,
				 uint8_t *completion_code,
				 uint8_t *comp_compatability_resp,
				 uint8_t *comp_compatability_resp_code,
				 bitfield32_t *update_option_flags_enabled,
				 uint16_t *estimated_time_req_fd)
{
	if (msg == NULL || completion_code == NULL ||
	    comp_compatability_resp == NULL ||
	    comp_compatability_resp_code == NULL ||
	    update_option_flags_enabled == NULL ||
	    estimated_time_req_fd == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != sizeof(struct pldm_update_component_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*completion_code = msg->payload[0];

	if (*completion_code != PLDM_SUCCESS) {
		return *completion_code;
	}

	struct pldm_update_component_resp *response =
	    (struct pldm_update_component_resp *)msg->payload;

	if (response == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (!check_compatability_resp_valid(
		response->comp_compatability_resp)) {
		return PLDM_ERROR_INVALID_DATA;
	}
	if (!check_compatability_resp_code_valid(
		response->comp_compatability_resp_code)) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*comp_compatability_resp = response->comp_compatability_resp;

	*comp_compatability_resp_code = response->comp_compatability_resp_code;

	update_option_flags_enabled->value =
	    le32toh(response->update_option_flags_enabled.value);

	*estimated_time_req_fd = le16toh(response->estimated_time_req_fd);

	return PLDM_SUCCESS;
}

int encode_get_status_req(const uint8_t instance_id, struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	return (encode_pldm_header_only(PLDM_REQUEST, instance_id, PLDM_FWUP,
					PLDM_GET_STATUS, msg));
}

/** @brief Check validity of current or previous status in GetStatus command
 *
 *	@param[in] state - current or previous different state machine state of
 *the FD
 *	@return validity
 */
static bool check_state_validity(const uint8_t state)
{
	switch (state) {
	case FD_IDLE:
	case FD_LEARN_COMPONENTS:
	case FD_READY_XFER:
	case FD_DOWNLOAD:
	case FD_VERIFY:
	case FD_APPLY:
	case FD_ACTIVATE:
		return true;
	default:
		return false;
	}
}

/** @brief Check validity of aux state in GetStatus command
 *
 *	@param[in] aux_state - provides additional information to the UA to
 *describe the current operation state of the FD
 *	@return validity
 */
static bool check_aux_state_validity(const uint8_t aux_state)
{
	switch (aux_state) {
	case FD_OPERATION_IN_PROGRESS:
	case FD_OPERATION_SUCCESSFUL:
	case FD_OPERATION_FAILED:
	case FD_WAIT:
		return true;
	default:
		return false;
	}
}

/** @brief Check validity of aux state status in GetStatus command
 *
 *	@param[in] aux_state_status - aux state status
 *	@return validity
 */
static bool check_aux_state_status_validity(const uint8_t aux_state_status)
{
	if (aux_state_status == FD_AUX_STATE_IN_PROGRESS_OR_SUCCESS ||
	    aux_state_status == FD_TIMEOUT ||
	    aux_state_status == FD_GENERIC_ERROR) {
		return true;
	} else if (aux_state_status >= FD_VENDOR_DEFINED_STATUS_CODE_START &&
		   aux_state_status <= FD_VENDOR_DEFINED_STATUS_CODE_END) {
		return true;
	}
	return false;
}

/** @brief Check validity of reason code in GetStatus command
 *
 *	@param[in] reason_code - Provides the reason for why the current state
 *entered the IDLE state
 *	@return validity
 */
static bool check_reason_code_validity(const uint8_t reason_code)
{

	switch (reason_code) {
	case FD_INITIALIZATION:
	case FD_ACTIVATE_FW_RECEIVED:
	case FD_CANCEL_UPDATE_RECEIVED:
	case FD_TIMEOUT_LEARN_COMPONENT:
	case FD_TIMEOUT_READY_XFER:
	case FD_TIMEOUT_DOWNLOAD:
	case FD_TIMEOUT_VERIFY:
	case FD_TIMEOUT_APPLY:
		return true;
	default:
		if (reason_code >= FD_STATUS_VENDOR_DEFINED_MIN) {
			return true;
		}
		return false;
	}
}

/** @brief Check validity of Response message in GetStatus command
 *
 *	@param[in] response - Response message
 *	@return validity
 */
static bool
check_response_msg_validity(const struct pldm_get_status_resp *response)
{
	if (response == NULL) {
		return false;
	}
	if (!check_state_validity(response->current_state)) {
		return false;
	}
	if (!check_state_validity(response->previous_state)) {
		return false;
	}
	if (!check_aux_state_validity(response->aux_state)) {
		return false;
	}
	if (!check_aux_state_status_validity(response->aux_state_status)) {
		return false;
	}
	if (response->progress_percent > FW_UPDATE_MAX_PROGRESS_PERCENT) {
		return false;
	}
	if (!check_reason_code_validity(response->reason_code)) {
		return false;
	}
	return true;
}

int decode_get_status_resp(const struct pldm_msg *msg,
			   const size_t payload_length,
			   uint8_t *completion_code, uint8_t *current_state,
			   uint8_t *previous_state, uint8_t *aux_state,
			   uint8_t *aux_state_status, uint8_t *progress_percent,
			   uint8_t *reason_code,
			   bitfield32_t *update_option_flags_enabled)
{
	if (msg == NULL || completion_code == NULL || current_state == NULL ||
	    previous_state == NULL || aux_state == NULL ||
	    aux_state_status == NULL || progress_percent == NULL ||
	    reason_code == NULL || update_option_flags_enabled == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg->payload[0];

	if (PLDM_SUCCESS != *completion_code) {
		return *completion_code;
	}
	if (payload_length != sizeof(struct pldm_get_status_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	struct pldm_get_status_resp *response =
	    (struct pldm_get_status_resp *)msg->payload;

	if (response == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (!check_response_msg_validity(response)) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*current_state = response->current_state;
	*previous_state = response->previous_state;
	*aux_state = response->aux_state;
	*aux_state_status = response->aux_state_status;
	*progress_percent = response->progress_percent;
	*reason_code = response->reason_code;
	update_option_flags_enabled->value =
	    le32toh(response->update_option_flags_enabled.value);

	return PLDM_SUCCESS;
}

int encode_cancel_update_req(const uint8_t instance_id, struct pldm_msg *msg)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	return (encode_pldm_header_only(PLDM_REQUEST, instance_id, PLDM_FWUP,
					PLDM_CANCEL_UPDATE, msg));
}

/** @brief Check whether non functioning component indication is valid
 *
 *  @return true if is from below mentioned values, false if not
 */
static bool check_non_functioning_component_indication_valid(
    const bool8_t non_functioning_component_indication)
{
	switch (non_functioning_component_indication) {
	case COMPONENTS_FUNCTIONING:
	case COMPONENTS_NOT_FUNCTIONING:
		return true;

	default:
		return false;
	}
}

int decode_cancel_update_resp(const struct pldm_msg *msg,
			      const size_t payload_len,
			      uint8_t *completion_code,
			      bool8_t *non_functioning_component_indication,
			      bitfield64_t *non_functioning_component_bitmap)
{
	if (msg == NULL || completion_code == NULL ||
	    non_functioning_component_indication == NULL ||
	    non_functioning_component_bitmap == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_len != sizeof(struct pldm_cancel_update_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	*completion_code = msg->payload[0];
	if (PLDM_SUCCESS != *completion_code) {
		return *completion_code;
	}

	struct pldm_cancel_update_resp *response =
	    (struct pldm_cancel_update_resp *)msg->payload;

	if (response == NULL) {
		return PLDM_ERROR_INVALID_DATA;
		;
	}

	if (!check_non_functioning_component_indication_valid(
		response->non_functioning_component_indication)) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*non_functioning_component_indication =
	    response->non_functioning_component_indication;

	if (*non_functioning_component_indication) {
		non_functioning_component_bitmap->value =
		    le64toh(response->non_functioning_component_bitmap);
	}

	return PLDM_SUCCESS;
}