#include "pldm_alloc.h"
#include "pldm_base_responder.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace libpldm
{

class PldmBaseResponderTest : public ::testing::Test
{
      protected:
	PldmBaseResponderTest() : type_capabilities({})
	{
		pldm_state.used = true;
		pldm_state.tid = 11;
		pldm_state.capabilities = &type_capabilities;
	};

	struct pldm_all_type_capabilities type_capabilities;
	struct pldm_responder_state pldm_state;
};

TEST_F(PldmBaseResponderTest, SetBaseCapabilities)
{
	EXPECT_TRUE(type_capabilities.type[PLDM_BASE] == nullptr);
	EXPECT_TRUE(
	    base_responder_add_supported_type(&type_capabilities, PLDM_BASE));
	EXPECT_TRUE(type_capabilities.type[PLDM_BASE]);
}

TEST_F(PldmBaseResponderTest, GetTid)
{
	struct pldm_responder_response ret;
	struct pldm_msg request;
	request.hdr.request = 1;
	request.hdr.datagram = 0;
	request.hdr.instance_id = 10;
	request.hdr.header_ver = 0;
	request.hdr.type = PLDM_BASE;
	request.hdr.command = PLDM_GET_TID;

	EXPECT_EQ(pldm_base_handle_request(&pldm_state, &request,
					   sizeof(request), &ret),
		  PLDM_SUCCESS);
	EXPECT_EQ(ret.len, sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES);
	EXPECT_TRUE(ret.payload);

	pldm_msg *resp = reinterpret_cast<pldm_msg *>(ret.payload);
	pldm_get_tid_resp *tid_resp =
	    reinterpret_cast<pldm_get_tid_resp *>(resp->payload);

	EXPECT_EQ(resp->hdr.request, 0);
	EXPECT_EQ(resp->hdr.instance_id, request.hdr.instance_id);
	EXPECT_EQ(resp->hdr.type, PLDM_BASE);
	EXPECT_EQ(resp->hdr.command, PLDM_GET_TID);
	EXPECT_EQ(tid_resp->completion_code, PLDM_SUCCESS);
	EXPECT_EQ(tid_resp->tid, pldm_state.tid);
	__pldm_free(ret.payload);
}

TEST_F(PldmBaseResponderTest, GetPLDMTypes)
{
	EXPECT_TRUE(
	    base_responder_add_supported_type(&type_capabilities, PLDM_BASE));
	EXPECT_TRUE(
	    base_responder_add_supported_type(&type_capabilities, PLDM_RDE));

	struct pldm_responder_response ret;
	struct pldm_msg request;
	request.hdr.request = 1;
	request.hdr.datagram = 0;
	request.hdr.instance_id = 10;
	request.hdr.header_ver = 0;
	request.hdr.type = PLDM_BASE;
	request.hdr.command = PLDM_GET_PLDM_TYPES;

	EXPECT_EQ(pldm_base_handle_request(&pldm_state, &request,
					   sizeof(request), &ret),
		  PLDM_SUCCESS);
	EXPECT_EQ(ret.len, sizeof(pldm_msg_hdr) + sizeof(pldm_get_types_resp));
	EXPECT_TRUE(ret.payload);

	pldm_msg *resp = reinterpret_cast<pldm_msg *>(ret.payload);
	pldm_get_types_resp *type_resp =
	    reinterpret_cast<pldm_get_types_resp *>(resp->payload);

	EXPECT_EQ(resp->hdr.request, 0);
	EXPECT_EQ(resp->hdr.instance_id, request.hdr.instance_id);
	EXPECT_EQ(resp->hdr.type, PLDM_BASE);
	EXPECT_EQ(resp->hdr.command, PLDM_GET_PLDM_TYPES);
	EXPECT_EQ(type_resp->completion_code, PLDM_SUCCESS);
	EXPECT_EQ(type_resp->types[0].byte,
		  ((1 << PLDM_RDE) | (1 << PLDM_BASE)));
	for (int i = 1; i < 8; ++i) {
		EXPECT_EQ(type_resp->types[i].byte, 0);
	}
	__pldm_free(ret.payload);
}

} // namespace libpldm
