#include "pldm_base_responder.h"

#include "pldm_alloc.h"

#include <stdio.h>

static const uint8_t pldm_base_type_commands[] = {PLDM_GET_TID,
						  PLDM_GET_PLDM_TYPES};

static const struct pldm_type_details pldm_base_type = {
    .type = PLDM_BASE,
    .version = 0xf1f0f000,
    .number_of_commands = sizeof(pldm_base_type_commands),
    .supported_command = pldm_base_type_commands,
};

// static const uint8_t pldm_platform_type_commands[] = {};

static const struct pldm_type_details pldm_platform_type = {
    .type = PLDM_PLATFORM,
    .version = 0xf1f2f000,
    .number_of_commands = 0,
    .supported_command = NULL,
};

// static const uint8_t pldm_rde_type_commands[] = {};

static const struct pldm_type_details pldm_rde_type = {
    .type = PLDM_RDE,
    .version = 0xf1f0f000,
    .number_of_commands = 0,
    .supported_command = NULL,
};

static uint8_t pldm_base_only_cc_resp(const struct pldm_msg *request,
				      struct pldm_responder_response *response,
				      uint8_t cc)
{
	response->len = 0;
	response->payload = NULL;

	size_t res_payload_siz = sizeof(struct pldm_msg);
	struct pldm_msg *res_payload =
	    (struct pldm_msg *)__pldm_alloc(sizeof(struct pldm_msg));
	if (!res_payload) {
		fprintf(stderr, "Failed to allocate memory for completion code "
				"only response\n");
		return PLDM_ERROR;
	}

	if (encode_cc_only_resp(request->hdr.instance_id, request->hdr.type,
				request->hdr.command, cc,
				res_payload) != PLDM_SUCCESS) {
		fprintf(stderr,
			"Failed to encode the completion code only response\n");
		__pldm_free(res_payload);
		return PLDM_ERROR;
	}

	response->len = res_payload_siz;
	response->payload = res_payload;
	return PLDM_SUCCESS;
}

static uint8_t pldm_base_get_tid(struct pldm_responder_state *state,
				 const struct pldm_msg *request,
				 struct pldm_responder_response *response)
{
	response->len = 0;
	response->payload = NULL;

	size_t res_payload_siz =
	    sizeof(struct pldm_msg_hdr) + sizeof(struct pldm_get_tid_resp);
	struct pldm_msg *res_payload =
	    (struct pldm_msg *)__pldm_alloc(res_payload_siz);
	if (!res_payload) {
		fprintf(stderr,
			"Failed to allocate memory for GetTID response\n");
		return PLDM_ERROR;
	}

	if (encode_get_tid_resp(request->hdr.instance_id, PLDM_SUCCESS,
				state->tid, res_payload) != PLDM_SUCCESS) {
		fprintf(stderr, "Failed to encode the GetTID response\n");
		__pldm_free(res_payload);
		return PLDM_ERROR;
	}

	response->len = res_payload_siz;
	response->payload = res_payload;
	return PLDM_SUCCESS;
}

static uint8_t
pldm_base_get_pldm_types(struct pldm_responder_state *state,
			 const struct pldm_msg *request,
			 struct pldm_responder_response *response)
{
	response->len = 0;
	response->payload = NULL;

	size_t res_payload_siz =
	    sizeof(struct pldm_msg_hdr) + sizeof(struct pldm_get_types_resp);
	struct pldm_msg *res_payload =
	    (struct pldm_msg *)__pldm_alloc(res_payload_siz);
	if (!res_payload) {
		fprintf(
		    stderr,
		    "Failed to allocate memory for GetPLDMTypes response\n");
		return PLDM_ERROR;
	}

	bitfield8_t types[8] = {0};
	for (size_t i = 0; i < 8; ++i) {
		for (size_t j = 0; j < 8; ++j) {
			if (state->capabilities->type[i * 8 + j]) {
				types[i].byte = types[i].byte | 1 << j;
			}
		}
	}

	if (encode_get_types_resp(request->hdr.instance_id, PLDM_SUCCESS, types,
				  res_payload) != PLDM_SUCCESS) {
		fprintf(stderr, "Failed to encode the GetPLDMTypes response\n");
		__pldm_free(res_payload);
		return PLDM_ERROR;
	}

	response->len = res_payload_siz;
	response->payload = res_payload;
	return PLDM_SUCCESS;
}

bool base_responder_add_supported_type(
    struct pldm_all_type_capabilities *type_capabilities,
    enum pldm_supported_types type)
{
	switch (type) {
	case PLDM_BASE:
		type_capabilities->type[type] = &pldm_base_type;
		break;
	case PLDM_PLATFORM:
		type_capabilities->type[type] = &pldm_platform_type;
		break;
	case PLDM_RDE:
		type_capabilities->type[type] = &pldm_rde_type;
		break;
	default:
		fprintf(stderr, "Type: %d provided is not supported\n", type);
		return false;
	}
	return true;
}

uint8_t pldm_base_handle_request(struct pldm_responder_state *state,
				 const struct pldm_msg *request,
				 size_t request_size,
				 struct pldm_responder_response *response)
{
	(void)request_size;
	uint8_t ret;

	if (!request->hdr.request) {
		fprintf(stderr, "Request bit is not set.\n");
		return PLDM_ERROR;
	}

	if (request->hdr.type != PLDM_BASE) {
		fprintf(stderr, "This is not a PLDM base request. Type:%d\n",
			request->hdr.type);
		return PLDM_ERROR;
	}

	// TODO: Support for unacknowledged PLDM request messages or
	// asynchronous notifications.
	switch (request->hdr.command) {
	case PLDM_GET_TID:
		ret = pldm_base_get_tid(state, request, response);
		break;
	case PLDM_GET_PLDM_TYPES:
		ret = pldm_base_get_pldm_types(state, request, response);
		break;
	default:
		fprintf(
		    stderr,
		    "Invalid PLDM base command or command not supported: %d\n",
		    request->hdr.command);
		ret = PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
	}

	if (ret != PLDM_SUCCESS) {
		fprintf(stderr,
			"Failed to generate a response for the command: %d. "
			"Trying to "
			"respond with just the failure code.\n",
			request->hdr.command);
		return pldm_base_only_cc_resp(request, response, ret);
	}

	return ret;
}
