#include "libpldm/base.h"

#include "common/types.hpp"
#include "common/utils.hpp"
#include "mock_request.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::requester;
using namespace std::chrono;

using ::testing::AtLeast;
using ::testing::Between;
using ::testing::Exactly;
using ::testing::NiceMock;
using ::testing::Return;

class HandlerTest : public testing::Test
{
  protected:
    HandlerTest() :
        event(sdeventplus::Event::get_default()),
        dbusImplReq(pldm::utils::DBusHandler::getBus(),
                    "/xyz/openbmc_project/pldm")
    {}

    int fd = 0;
    mctp_eid_t eid = 0;
    sdeventplus::Event event;
    pldm::dbus_api::Requester dbusImplReq;

    /** @brief This function runs the sd_event_run in a loop till all the events
     *         in the testcase are dispatched and exits when there are no events
     *         for the timeout time.
     *
     *  @param[in] timeout - maximum time to wait for an event
     */
    void waitEventExpiry(milliseconds timeout)
    {
        while (1)
        {
            auto sleepTime = duration_cast<microseconds>(timeout);
            // Returns 0 on timeout
            if (!sd_event_run(event.get(), sleepTime.count()))
            {
                break;
            }
        }
    }

  public:
    bool nullResponse = false;
    bool validResponse = false;
    int callbackCount = 0;
    bool response2 = false;

    void pldmResponseCallBack(mctp_eid_t /*eid*/, const pldm_msg* response,
                              size_t respMsgLen)
    {
        if (response == nullptr && respMsgLen == 0)
        {
            nullResponse = true;
        }
        else
        {
            validResponse = true;
        }
        callbackCount++;
    }
};

TEST_F(HandlerTest, singleRequestResponseScenario)
{
    Handler<NiceMock<MockRequest>> reqHandler(
        fd, event, dbusImplReq, false, 90000, seconds(1), 2, milliseconds(100));
    pldm::Request request{};
    auto instanceId = dbusImplReq.getInstanceId(eid);
    auto rc = reqHandler.registerRequest(
        eid, instanceId, 0, 0, std::move(request),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    pldm::Response response(sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    auto responsePtr = reinterpret_cast<const pldm_msg*>(response.data());
    reqHandler.handleResponse(eid, instanceId, 0, 0, responsePtr,
                              sizeof(response));

    // handleResponse() will free the instance ID after calling the response
    // handler, so the same instance ID is granted next as well
    EXPECT_EQ(validResponse, true);
    EXPECT_EQ(instanceId, dbusImplReq.getInstanceId(eid));
}

TEST_F(HandlerTest, singleRequestInstanceIdTimerExpired)
{
    Handler<NiceMock<MockRequest>> reqHandler(
        fd, event, dbusImplReq, false, 90000, seconds(1), 2, milliseconds(100));
    pldm::Request request{};
    auto instanceId = dbusImplReq.getInstanceId(eid);
    auto rc = reqHandler.registerRequest(
        eid, instanceId, 0, 0, std::move(request),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // Waiting for 500ms so that the instance ID expiry callback is invoked
    waitEventExpiry(milliseconds(500));

    // cleanup() will free the instance ID after calling the response
    // handler will no response, so the same instance ID is granted next
    EXPECT_EQ(instanceId, dbusImplReq.getInstanceId(eid));
    EXPECT_EQ(nullResponse, true);
}

TEST_F(HandlerTest, multipleRequestResponseScenario)
{
    Handler<NiceMock<MockRequest>> reqHandler(
        fd, event, dbusImplReq, false, 90000, seconds(2), 2, milliseconds(100));
    pldm::Request request{};
    auto instanceId = dbusImplReq.getInstanceId(eid);
    auto rc = reqHandler.registerRequest(
        eid, instanceId, 0, 0, std::move(request),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    pldm::Request requestNxt{};
    auto instanceIdNxt = dbusImplReq.getInstanceId(eid);
    rc = reqHandler.registerRequest(
        eid, instanceIdNxt, 0, 0, std::move(requestNxt),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    pldm::Response response(sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    auto responsePtr = reinterpret_cast<const pldm_msg*>(response.data());
    reqHandler.handleResponse(eid, instanceIdNxt, 0, 0, responsePtr,
                              sizeof(response));
    EXPECT_EQ(validResponse, true);
    EXPECT_EQ(callbackCount, 1);
    validResponse = false;

    // Waiting for 500ms and handle the response for the first request, to
    // simulate a delayed response for the first request
    waitEventExpiry(milliseconds(500));

    reqHandler.handleResponse(eid, instanceId, 0, 0, responsePtr,
                              sizeof(response));

    EXPECT_EQ(validResponse, true);
    EXPECT_EQ(callbackCount, 2);
    EXPECT_EQ(instanceId, dbusImplReq.getInstanceId(eid));
}

Coroutine getTID(Handler<MockRequest>& handler, mctp_eid_t eid,
                 uint8_t instanceId, uint8_t& tid)
{
    pldm::Request request(sizeof(pldm_msg_hdr));
    const pldm_msg* responseMsg = NULL;
    size_t responseLen = 0;
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_get_tid_req(instanceId, requestMsg);
    EXPECT_EQ(rc, PLDM_SUCCESS);

    rc = co_await SendRecvPldmMsg<Handler<MockRequest>>(
        handler, eid, request, &responseMsg, &responseLen);
    EXPECT_EQ(rc, PLDM_SUCCESS);

    uint8_t cc = 0;
    rc = decode_get_tid_resp(responseMsg, responseLen, &cc, &tid);
    EXPECT_EQ(rc, PLDM_SUCCESS);

    co_return cc;
}

TEST_F(HandlerTest, requestResponseByCoroutine)
{
    Handler<MockRequest> reqHandler(fd, event, dbusImplReq, false, 90000,
                                    seconds(1), 2, milliseconds(100));

    uint8_t respTid = 0;
    auto instanceId = dbusImplReq.getInstanceId(eid);
    // Execute a coroutine to send getTID command. The coroutine is suspended
    // until reqHandler.handleResponse() is received.
    auto co = getTID(reqHandler, eid, instanceId, respTid);

    // Compose response message of getTID command
    uint8_t tid = 1;
    pldm::Response responseMsg(sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES);
    auto response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    encode_get_tid_resp(instanceId, PLDM_SUCCESS, tid, response);

    EXPECT_NE(tid, respTid);

    // Send response back to resume getTID coroutine to update respTid by
    // calling  reqHandler.handleResponse() manually
    reqHandler.handleResponse(eid, instanceId, PLDM_BASE, PLDM_GET_TID,
                              response,
                              responseMsg.size() - sizeof(pldm_msg_hdr));

    EXPECT_EQ(tid, respTid);
    co.handle.destroy();
}