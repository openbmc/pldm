#include "libpldm/pldm_base_requester.h"
#include "libpldm/base.h"
#include "libpldm/pldm.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

int currentRequestPtr;
int lastRequestPtr;

pldm_base_requester_rc_t pldm_base_start_discovery(struct requester_base_context *ctx)
{
	fprintf(stderr, "\nInitializing context\n");
	ctx->initialized = true;
	ctx->next_command = PLDM_GET_TID;
	ctx->command_status = COMMAND_NOT_STARTED;
	
	return PLDM_BASE_REQUESTER_SUCCESS;
}

pldm_base_requester_rc_t
pldm_base_get_next_request(struct requester_base_context *ctx,
			      uint8_t instance_id, struct pldm_msg *request)
{
	int rc;
	switch (ctx->next_command) {
	case PLDM_GET_TID:
		rc = encode_get_tid_req(instance_id, request);
		if (rc) {
			return PLDM_BASE_REQUESTER_ENCODING_REQUEST_FAILURE;
		}
		return PLDM_BASE_REQUESTER_SUCCESS;
	case PLDM_GET_PLDM_TYPES:
		rc = encode_get_types_req(instance_id, request);
		if (rc) {
			return PLDM_BASE_REQUESTER_ENCODING_REQUEST_FAILURE;
		}
		return PLDM_BASE_REQUESTER_SUCCESS;

	case PLDM_GET_PLDM_VERSION: {
		// ctx->command_pldm_type = PLDM_BASE;
		u_int8_t pldmType = ctx->command_pldm_type;
		rc = encode_get_version_req(instance_id, 0, PLDM_GET_FIRSTPART,
					    pldmType, request);
		if (rc) {
			return PLDM_BASE_REQUESTER_ENCODING_REQUEST_FAILURE;
		}
		return PLDM_BASE_REQUESTER_SUCCESS;
	}

	case PLDM_GET_PLDM_COMMANDS: {
		u_int8_t pldmType = ctx->command_pldm_type;
		ver32_t pldmVersion = ctx->pldm_versions[pldmType];
		rc = encode_get_commands_req(instance_id, pldmType, pldmVersion,
					     request);
		if (rc) {
			return PLDM_BASE_REQUESTER_ENCODING_REQUEST_FAILURE;
		}
		return PLDM_BASE_REQUESTER_SUCCESS;
	}

	default:
		return PLDM_BASE_REQUESTER_NO_NEXT_COMMAND_FOUND;
	}
}

// TODO: Call handlers for TID, Version, types and commands
pldm_base_requester_rc_t pldm_base_push_response(struct requester_base_context *ctx,
					   void *resp_msg, size_t resp_size)
{
	switch (ctx->next_command) {
	case PLDM_GET_TID: {
		uint8_t completionCode;
		uint8_t tid;
		int rc =
		    decode_get_tid_resp((struct pldm_msg *)resp_msg,
					resp_size - sizeof(struct pldm_msg_hdr),
					&completionCode, &tid);
		if (rc) {
			ctx->command_status = COMMAND_FAILED;
			return PLDM_BASE_REQUESTER_NOT_RESP_MSG;
		}
		ctx->tid = tid;
		ctx->command_status = COMMAND_COMPLETED;
		ctx->next_command = PLDM_GET_PLDM_TYPES;
		ctx->command_status = COMMAND_NOT_STARTED;
		return PLDM_BASE_REQUESTER_SUCCESS;
	}

	case PLDM_GET_PLDM_TYPES: {
		uint8_t completionCode;
		bitfield8_t types[PLDM_MAX_TYPES / 8];
		int rc = decode_get_types_resp((struct pldm_msg *)resp_msg,
					       resp_size -
						   sizeof(struct pldm_msg_hdr),
					       &completionCode, types);
		if (rc) {
			ctx->command_status = COMMAND_FAILED;
			return PLDM_BASE_REQUESTER_NOT_RESP_MSG;
		}
		for (int i = 0; i < (PLDM_MAX_TYPES / 8); i++) {
			ctx->pldm_types[i] = types[i].byte;
		}
		ctx->command_status = COMMAND_COMPLETED;

		// TODO: We would have to execute the PLDM_VERSION command for all the PLDM_TYPE
		// TODO: Remove this and add it to a queue for which version can be collected
		ctx->command_pldm_type = PLDM_BASE; 
		ctx->next_command = PLDM_GET_PLDM_VERSION;
		ctx->command_status = COMMAND_NOT_STARTED;
		return PLDM_BASE_REQUESTER_SUCCESS;
	}

	case PLDM_GET_PLDM_VERSION:
		// call pldm version handler - decode_get_version_resp()
		{
			ver32_t versionOut;
			uint8_t completionCode;
			uint8_t retFlag;
			uint32_t retTransferHandle;
			int rc = decode_get_version_resp(
			    (struct pldm_msg *)resp_msg,
			    resp_size - sizeof(struct pldm_msg_hdr),
			    &completionCode, &retTransferHandle, &retFlag,
			    &versionOut);

			if (rc) {
				return PLDM_BASE_REQUESTER_NOT_RESP_MSG;
			}
			uint8_t pldmReqType = ctx->command_pldm_type;
				
			ctx->pldm_versions[pldmReqType] = versionOut;
			ctx->command_status = COMMAND_COMPLETED;
			ctx->next_command = PLDM_GET_PLDM_COMMANDS;
			ctx->command_status = COMMAND_NOT_STARTED;
		}
		return PLDM_BASE_REQUESTER_SUCCESS;

	case PLDM_GET_PLDM_COMMANDS:
		// call pldm commands handler - decode_get_commands_resp()
		// Update context
		{
			uint8_t completionCode;
			bitfield8_t pldmCmds[PLDM_MAX_CMDS_PER_TYPE / 8];
			int rc = decode_get_commands_resp(
			    (struct pldm_msg *)resp_msg,
			    resp_size - sizeof(struct pldm_msg_hdr),
			    &completionCode, pldmCmds);
			if (rc) {
				return PLDM_BASE_REQUESTER_NOT_RESP_MSG;
			}

			for (int i = 0; i < (PLDM_MAX_CMDS_PER_TYPE / 8); i++) {
				ctx->pldm_commands[i] = pldmCmds[i].byte;
			}

			ctx->command_status = COMMAND_COMPLETED;
			ctx->next_command = PLDM_BASE_REQUESTER_NO_NEXT_COMMAND_FOUND;
			ctx->command_status = COMMAND_NOT_STARTED;
		}
		return PLDM_BASE_REQUESTER_SUCCESS;

	default:
		return PLDM_BASE_REQUESTER_NO_NEXT_COMMAND_FOUND;
	}

	return PLDM_BASE_REQUESTER_NOT_PLDM_BASE_MSG;
}

void processTidRequest() {}