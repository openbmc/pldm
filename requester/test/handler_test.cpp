#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "mock_request.hpp"
#include "requester/handler.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/base.h>
#include <libpldm/transport.h>

#include <sdbusplus/async/context.hpp>
#include <sdbusplus/async/stdexec/task.hpp>

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

TEST_F(HandlerTest, singleRequestResponseScenarioUsingCoroutine)
{
    sdbusplus::async::context ctx;
    Handler<NiceMock<MockRequest>> reqHandler(pldmTransport, event,
                                              instanceIdDb, false, seconds(1),
                                              2, milliseconds(100));

    auto instanceId = instanceIdDb.next(eid);
    EXPECT_EQ(instanceId, 0);

    ctx.spawn(stdexec::just() | stdexec::let_value([&]() -> exec::task<void> {
        int rc;
        pldm::Request request(sizeof(pldm_msg_hdr) + sizeof(uint8_t), 0);
        pldm::Response response;

        auto requestPtr = reinterpret_cast<pldm_msg*>(request.data());
        requestPtr->hdr.instance_id = instanceId;

        std::tie(rc, response) =
            co_await reqHandler.sendRecvMsg(eid, std::move(request));
        EXPECT_EQ(rc, PLDM_SUCCESS);
        EXPECT_NE(response.size(), 0);

        this->pldmResponseCallBack(
            eid, reinterpret_cast<const pldm_msg*>(response.data()),
            response.size());

        EXPECT_EQ(validResponse, true);

        ctx.request_stop();
    }));

    ctx.spawn(stdexec::just() | stdexec::then([&]() {
        pldm::Response mockResponse(sizeof(pldm_msg_hdr) + sizeof(uint8_t), 0);
        auto mockResponsePtr =
            reinterpret_cast<const pldm_msg*>(mockResponse.data());
        reqHandler.handleResponse(eid, instanceId, 0, 0, mockResponsePtr,
                                  sizeof(mockResponse));
    }));

    ctx.run();
}

TEST_F(HandlerTest, asyncRequestResponseByCoroutine)
{
    struct _
    {
        static exec::task<uint8_t> getTIDTask(Handler<MockRequest>& handler,
                                              mctp_eid_t eid,
                                              uint8_t instanceId, uint8_t& tid)
        {
            pldm::Request request(sizeof(pldm_msg_hdr), 0);
            auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());

            auto rc = encode_get_tid_req(instanceId, requestMsg);
            EXPECT_EQ(rc, PLDM_SUCCESS);

            pldm::Response response;
            std::tie(rc, response) =
                co_await handler.sendRecvMsg(eid, std::move(request));
            EXPECT_EQ(rc, PLDM_SUCCESS);
            EXPECT_NE(response.size(), 0);

            uint8_t cc = 0;
            rc = decode_get_tid_resp(
                reinterpret_cast<const pldm_msg*>(response.data()),
                response.size() - sizeof(pldm_msg_hdr), &cc, &tid);
            EXPECT_EQ(rc, PLDM_SUCCESS);

            co_return cc;
        }
    };

    sdbusplus::async::context ctx;
    Handler<MockRequest> reqHandler(pldmTransport, event, instanceIdDb, false,
                                    seconds(1), 2, milliseconds(100));
    auto instanceId = instanceIdDb.next(eid);

    uint8_t expectedTid = 1;

    // Execute a coroutine to send getTID command. The coroutine is suspended
    // until reqHandler.handleResponse() is received.
    ctx.spawn(stdexec::just() | stdexec::let_value([&]() -> exec::task<void> {
        uint8_t respTid = 0;

        co_await _::getTIDTask(reqHandler, eid, instanceId, respTid);

        EXPECT_EQ(expectedTid, respTid);

        ctx.request_stop();
    }));

    ctx.spawn(stdexec::just() | stdexec::then([&]() {
        pldm::Response mockResponse(
            sizeof(pldm_msg_hdr) + PLDM_GET_TID_RESP_BYTES, 0);
        auto mockResponseMsg = reinterpret_cast<pldm_msg*>(mockResponse.data());

        // Compose response message of getTID command
        encode_get_tid_resp(instanceId, PLDM_SUCCESS, expectedTid,
                            mockResponseMsg);

        // Send response back to resume getTID coroutine to update respTid by
        // calling  reqHandler.handleResponse() manually
        reqHandler.handleResponse(eid, instanceId, PLDM_BASE, PLDM_GET_TID,
                                  mockResponseMsg,
                                  mockResponse.size() - sizeof(pldm_msg_hdr));
    }));

    ctx.run();
}
