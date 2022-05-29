#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "mock_request.hpp"
#include "requester/handler.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/base.h>
#include <libpldm/transport.h>

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
    HandlerTest() : event(sdeventplus::Event::get_default()), instanceIdDb() {}

    int fd = 0;
    mctp_eid_t eid = 0;
    PldmTransport* pldmTransport = nullptr;
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;

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
    Handler<NiceMock<MockRequest>> reqHandler(pldmTransport, event,
                                              instanceIdDb, false, seconds(1),
                                              2, milliseconds(100));
    pldm::Request request{};
    auto instanceId = instanceIdDb.next(eid);
    EXPECT_EQ(instanceId, 0);
    auto rc = reqHandler.registerRequest(
        eid, instanceId, 0, 0, std::move(request),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    pldm::Response response(sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    auto responsePtr = reinterpret_cast<const pldm_msg*>(response.data());
    reqHandler.handleResponse(eid, instanceId, 0, 0, responsePtr,
                              sizeof(response));

    EXPECT_EQ(validResponse, true);
}

TEST_F(HandlerTest, singleRequestInstanceIdTimerExpired)
{
    Handler<NiceMock<MockRequest>> reqHandler(pldmTransport, event,
                                              instanceIdDb, false, seconds(1),
                                              2, milliseconds(100));
    pldm::Request request{};
    auto instanceId = instanceIdDb.next(eid);
    EXPECT_EQ(instanceId, 0);
    auto rc = reqHandler.registerRequest(
        eid, instanceId, 0, 0, std::move(request),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    // Waiting for 500ms so that the instance ID expiry callback is invoked
    waitEventExpiry(milliseconds(500));

    EXPECT_EQ(nullResponse, true);
}

TEST_F(HandlerTest, multipleRequestResponseScenario)
{
    Handler<NiceMock<MockRequest>> reqHandler(pldmTransport, event,
                                              instanceIdDb, false, seconds(2),
                                              2, milliseconds(100));
    pldm::Request request{};
    auto instanceId = instanceIdDb.next(eid);
    EXPECT_EQ(instanceId, 0);
    auto rc = reqHandler.registerRequest(
        eid, instanceId, 0, 0, std::move(request),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    pldm::Request requestNxt{};
    auto instanceIdNxt = instanceIdDb.next(eid);
    EXPECT_EQ(instanceIdNxt, 1);
    rc = reqHandler.registerRequest(
        eid, instanceIdNxt, 0, 0, std::move(requestNxt),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));
    EXPECT_EQ(rc, PLDM_SUCCESS);

    pldm::Response response(sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    auto responsePtr = reinterpret_cast<const pldm_msg*>(response.data());
    reqHandler.handleResponse(eid, instanceId, 0, 0, responsePtr,
                              sizeof(response));
    EXPECT_EQ(validResponse, true);
    EXPECT_EQ(callbackCount, 1);
    validResponse = false;

    // Waiting for 500ms and handle the response for the first request, to
    // simulate a delayed response for the first request
    waitEventExpiry(milliseconds(500));

    reqHandler.handleResponse(eid, instanceIdNxt, 0, 0, responsePtr,
                              sizeof(response));

    EXPECT_EQ(validResponse, true);
    EXPECT_EQ(callbackCount, 2);
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
    Handler<MockRequest> reqHandler(pldmTransport, event, instanceIdDb, false,
                                    seconds(1), 2, milliseconds(100));

    uint8_t respTid = 0;
    auto instanceId = instanceIdDb.next(eid);

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
}
