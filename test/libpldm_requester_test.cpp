#include "libpldm/requester/pldm.h"
#include "libpldm/base.h"
#include "libpldm/platform.h"

#include <stdlib.h>

#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/socket.h>

uint8_t instanceId = 20;
uint8_t mctpEid = 10;
constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;
uint8_t mctpPrefix[2] = {mctpEid, MCTP_MSG_TYPE_PLDM};

ssize_t sendmsg(int /*sockfd*/, const struct msghdr* /*msg*/, int /*flags*/)
{
    return 0;
}

ssize_t recv(int /*sockfd*/, void* /*buf*/, size_t /*len*/, int /*flags*/)
{
    return sizeof(mctpPrefix) + sizeof(pldm_msg_hdr) + PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES;;
}

ssize_t recvmsg(int /*sockfd*/, struct msghdr* msg, int /*flags*/)
{
    memcpy(msg->msg_iov[0].iov_base, mctpPrefix, sizeof(mctpPrefix));
    std::array<uint8_t, sizeof(pldm_msg_hdr) + 1> resp{};
    pldm_msg *req = reinterpret_cast<pldm_msg*>(resp.data());
    encode_set_state_effecter_states_resp(instanceId, PLDM_SUCCESS, req);
    memcpy(msg->msg_iov[1].iov_base, resp.data(), sizeof(resp));
    return sizeof(mctpPrefix) + sizeof(pldm_msg_hdr) + PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES;;
}

TEST(RequesterAPI, SendRecv)
{
    uint8_t effecterCount = 1;
    uint8_t effecterId = 1;
    uint8_t state = 0;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + sizeof(effecterId) +
                            sizeof(effecterCount) +
                            sizeof(set_effecter_state_field)>
        requestMsg{};
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    set_effecter_state_field stateField{PLDM_REQUEST_SET, state};
    auto rc = encode_set_state_effecter_states_req(instanceId, effecterId, effecterCount,
                                                   &stateField, request);
    ASSERT_EQ(rc, PLDM_SUCCESS);

    rc = pldm_send(0, 0, requestMsg.data(), requestMsg.size());
    ASSERT_EQ(rc, 0);

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};
    rc = pldm_send_recv(mctpEid, 0, requestMsg.data(), requestMsg.size(),
                        &responseMsg, &responseMsgSize);
    ASSERT_EQ(rc, 0);

    rc = pldm_recv_any(mctpEid, 0,
                        &responseMsg, &responseMsgSize);
    ASSERT_EQ(rc, 0);

    rc = pldm_recv(mctpEid, 0, instanceId,
                        &responseMsg, &responseMsgSize);
    ASSERT_EQ(rc, 0);

    rc = pldm_recv_any(0, 0,
                        &responseMsg, &responseMsgSize);
    ASSERT_EQ(rc, -1);

    rc = pldm_recv(mctpEid, 0, 0,
                        &responseMsg, &responseMsgSize);
    ASSERT_EQ(rc, -1);
}
