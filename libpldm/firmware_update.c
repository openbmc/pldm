#include <endian.h>
#include <string.h>

#include "firmware_update.h"
int encode_query_device_identifiers_req(const uint8_t instance_id,
					struct pldm_msg *msg,
					const size_t payload_length)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_header_info header = {0};
	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_FWU;
	header.command = PLDM_QUERY_DEVICE_IDENTIFIERS;
	int rc = pack_pldm_header(&header, &(msg->hdr));
	if (PLDM_SUCCESS != rc) {
		return rc;
	}

	return PLDM_SUCCESS;
}
int decode_query_device_identifiers_resp(const struct pldm_msg *msg,
					 const size_t payload_length,
					 uint8_t *completion_code,
					 uint32_t *device_identifiers_len,
					 uint8_t *descriptor_count,
					 struct variable_field *descriptor_data)
{
	if (msg == NULL || completion_code == NULL ||
	    device_identifiers_len == NULL || descriptor_count == NULL ||
	    descriptor_data == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	*completion_code = msg->payload[0];
	if (PLDM_SUCCESS != *completion_code) {
		return *completion_code;
	}

	if (payload_length < sizeof(struct query_device_identifiers_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct query_device_identifiers_resp *response =
	    (struct query_device_identifiers_resp *)msg->payload;
	*device_identifiers_len = htole32(response->device_identifiers_len);

	if (*device_identifiers_len < PLDM_FWU_MIN_DESCRIPTOR_IDENTIFIERS_LEN) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	size_t resp_len = sizeof(struct query_device_identifiers_resp);

	if (payload_length != resp_len + *device_identifiers_len) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	*descriptor_count = response->descriptor_count;

	if (*descriptor_count == 0) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (descriptor_data->length < *device_identifiers_len) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	memset((void *)descriptor_data->ptr, 0, descriptor_data->length);
	memcpy((void *)descriptor_data->ptr, msg->payload + resp_len,
	       *device_identifiers_len);

	return PLDM_SUCCESS;
}
int encode_get_firmware_parameters_req(const uint8_t instance_id,
				       struct pldm_msg *msg,
				       const size_t payload_length)
{
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_GET_FIRMWARE_PARAMENTERS_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pldm_header_info header = {0};
	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_FWU;
	header.command = PLDM_GET_FIRMWARE_PARAMENTERS;
	int rc = pack_pldm_header(&header, &(msg->hdr));
	if (PLDM_SUCCESS != rc) {
		return rc;
	}

	return PLDM_SUCCESS;
}
int decode_get_firmware_parameters_comp_resp(
    const uint8_t *msg, const size_t payload_length,
    struct component_parameter_table *component_data,
    struct variable_field *active_comp_ver_str,
    struct variable_field *pending_comp_ver_str)
{
	if (msg == NULL || component_data == NULL ||
	    active_comp_ver_str == NULL || pending_comp_ver_str == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length < sizeof(struct component_parameter_table)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct component_parameter_table *component_resp =
	    (struct component_parameter_table *)(msg);
	if (component_resp->active_comp_ver_str_len == 0) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	size_t resp_len = sizeof(struct component_parameter_table) +
			  component_resp->active_comp_ver_str_len +
			  component_resp->pending_comp_ver_str_len;

	if (payload_length < resp_len) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	component_data->comp_classification =
	    htole16(component_resp->comp_classification);
	component_data->comp_identifier =
	    htole16(component_resp->comp_identifier);
	component_data->comp_classification_index =
	    component_resp->comp_classification_index;
	component_data->active_comp_comparison_stamp =
	    htole32(component_resp->active_comp_comparison_stamp);
	component_data->active_comp_ver_str_type =
	    component_resp->active_comp_ver_str_type;
	component_data->active_comp_ver_str_len =
	    component_resp->active_comp_ver_str_len;
	component_data->active_comp_release_date =
	    htole64(component_resp->active_comp_release_date);
	component_data->pending_comp_comparison_stamp =
	    htole32(component_resp->pending_comp_comparison_stamp);
	component_data->pending_comp_ver_str_type =
	    component_resp->pending_comp_ver_str_type;
	component_data->pending_comp_ver_str_len =
	    component_resp->pending_comp_ver_str_len;
	component_data->pending_comp_release_date =
	    htole64(component_resp->pending_comp_release_date);
	component_data->comp_activation_methods =
	    htole16(component_resp->comp_activation_methods);
	component_data->capabilities_during_update =
	    htole16(component_resp->capabilities_during_update);

	active_comp_ver_str->ptr =
	    msg + sizeof(struct component_parameter_table);
	active_comp_ver_str->length = component_resp->active_comp_ver_str_len;

	if (component_resp->pending_comp_ver_str_len != 0) {

		pending_comp_ver_str->ptr =
		    msg + sizeof(struct component_parameter_table) +
		    component_resp->active_comp_ver_str_len;
		pending_comp_ver_str->length =
		    component_resp->pending_comp_ver_str_len;
	} else {
		pending_comp_ver_str->ptr = NULL;
		pending_comp_ver_str->length = 0;
	}
	return PLDM_SUCCESS;
}
int decode_get_firmware_parameters_comp_img_set_resp(
    const struct pldm_msg *msg, const size_t payload_length,
    struct get_firmware_parameters_resp *resp_data,
    struct variable_field *active_comp_image_set_ver_str,
    struct variable_field *pending_comp_image_set_ver_str)
{
	if (msg == NULL || resp_data == NULL ||
	    active_comp_image_set_ver_str == NULL ||
	    pending_comp_image_set_ver_str == NULL) {

		return PLDM_ERROR_INVALID_DATA;
	}

	const size_t min_resp_len = sizeof(struct get_firmware_parameters_resp);

	if (payload_length < min_resp_len) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct get_firmware_parameters_resp *response =
	    (struct get_firmware_parameters_resp *)msg->payload;

	if (PLDM_SUCCESS != response->completion_code) {
		return response->completion_code;
	}

	if (response->active_comp_image_set_ver_str_len == 0) {
		return PLDM_ERROR_INVALID_DATA;
	}

	size_t resp_len = sizeof(struct get_firmware_parameters_resp) +
			  response->active_comp_image_set_ver_str_len +
			  response->pending_comp_image_set_ver_str_len;

	if (payload_length < resp_len) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	resp_data->capabilities_during_update =
	    htole32(response->capabilities_during_update);
	resp_data->comp_count = htole16(response->comp_count);

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
	    msg->payload + sizeof(struct get_firmware_parameters_resp);
	active_comp_image_set_ver_str->length =
	    resp_data->active_comp_image_set_ver_str_len;

	if (resp_data->pending_comp_image_set_ver_str_len != 0) {
		pending_comp_image_set_ver_str->ptr =
		    msg->payload + sizeof(struct get_firmware_parameters_resp) +
		    resp_data->active_comp_image_set_ver_str_len;
		pending_comp_image_set_ver_str->length =
		    resp_data->pending_comp_image_set_ver_str_len;
	} else {
		pending_comp_image_set_ver_str->ptr = NULL;
		pending_comp_image_set_ver_str->length = 0;
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
	case COMP_VER_STR_TYPE_UNKNOWN:
	case COMP_ASCII:
	case COMP_UTF_8:
	case COMP_UTF_16:
	case COMP_UTF_16LE:
	case COMP_UTF_16BE:
		return true;

	default:
		return false;
	}
}

int encode_request_update_req(const uint8_t instance_id, struct pldm_msg *msg,
			      const size_t payload_length,
			      const struct request_update_req *data,
			      struct variable_field *comp_img_set_ver_str)
{
	if (msg == NULL || data == NULL || comp_img_set_ver_str == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length < sizeof(struct request_update_req)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	int rc = encode_pldm_header(instance_id, PLDM_FWU, PLDM_REQUEST_UPDATE,
				    PLDM_REQUEST, msg);

	if (PLDM_SUCCESS != rc) {
		return rc;
	}

	struct request_update_req *request =
	    (struct request_update_req *)msg->payload;

	if (request == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (data->max_transfer_size < PLDM_FWU_BASELINE_TRANSFER_SIZE) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	request->max_transfer_size = htole32(data->max_transfer_size);
	request->no_of_comp = htole16(data->no_of_comp);

	if (data->max_outstand_transfer_req < MIN_OUTSTANDING_REQ) {
		return PLDM_ERROR_INVALID_DATA;
	}

	request->max_outstand_transfer_req = data->max_outstand_transfer_req;
	request->pkg_data_len = htole16(data->pkg_data_len);

	if (!check_comp_ver_str_type_valid(data->comp_image_set_ver_str_type)) {
		return PLDM_ERROR_INVALID_DATA;
	}

	request->comp_image_set_ver_str_type =
	    data->comp_image_set_ver_str_type;
	request->comp_image_set_ver_str_len = data->comp_image_set_ver_str_len;

	if (payload_length !=
	    sizeof(struct request_update_req) + comp_img_set_ver_str->length) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	if (comp_img_set_ver_str->ptr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	memcpy(msg->payload + sizeof(struct request_update_req),
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

	*completion_code = msg->payload[0];

	if (*completion_code != PLDM_SUCCESS) {
		return *completion_code;
	}

	if (payload_length != sizeof(struct request_update_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct request_update_resp *response =
	    (struct request_update_resp *)msg->payload;

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
	case COMP_CAN_BE_UPDATEABLE:
	case COMP_MAY_BE_UPDATEABLE:
		return true;

	default:
		return false;
	}
}

/** @brief Check whether Component Classification is valid
 *
 *  @return true if is from below mentioned values, false if not
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
		return true;

	default:
		return false;
	}
}

int encode_pass_component_table_req(const uint8_t instance_id,
				    struct pldm_msg *msg,
				    const size_t payload_length,
				    const struct pass_component_table_req *data,
				    struct variable_field *comp_ver_str)
{
	if (msg == NULL || data == NULL || comp_ver_str == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length < sizeof(struct pass_component_table_req)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	int rc =
	    encode_pldm_header(instance_id, PLDM_FWU, PLDM_PASS_COMPONENT_TABLE,
			       PLDM_REQUEST, msg);

	if (PLDM_SUCCESS != rc) {
		return rc;
	}

	struct pass_component_table_req *request =
	    (struct pass_component_table_req *)msg->payload;

	if (request == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (!check_transfer_flag_valid(data->transfer_flag)) {
		return PLDM_INVALID_TRANSFER_OPERATION_FLAG;
	}

	request->transfer_flag = data->transfer_flag;

	if (!check_comp_classification_valid(
		htole16(data->comp_classification))) {
		return PLDM_ERROR_INVALID_DATA;
	}

	request->comp_classification = htole16(data->comp_classification);
	request->comp_identifier = htole16(data->comp_identifier);
	request->comp_classification_index = data->comp_classification_index;
	request->comp_comparison_stamp = htole32(data->comp_comparison_stamp);

	if (!check_comp_ver_str_type_valid(data->comp_ver_str_type)) {
		return PLDM_ERROR_INVALID_DATA;
	}

	request->comp_ver_str_type = data->comp_ver_str_type;
	request->comp_ver_str_len = data->comp_ver_str_len;

	if (payload_length !=
	    sizeof(struct pass_component_table_req) + comp_ver_str->length) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	if (comp_ver_str->ptr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	memcpy(msg->payload + sizeof(struct pass_component_table_req),
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

	*completion_code = msg->payload[0];

	if (*completion_code != PLDM_SUCCESS) {
		return *completion_code;
	}

	if (payload_length != sizeof(struct pass_component_table_resp)) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct pass_component_table_resp *response =
	    (struct pass_component_table_resp *)msg->payload;

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
