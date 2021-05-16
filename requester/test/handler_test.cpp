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
    Handler<NiceMock<MockRequest>> reqHandler(fd, event, dbusImplReq,
                                              seconds(1), 2, milliseconds(100));
    pldm::Request request{};
    auto instanceId = dbusImplReq.getInstanceId(eid);
    reqHandler.registerRequest(
        eid, instanceId, 0, 0, std::move(request),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));

    pldm::Response response(sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    auto responsePtr = reinterpret_cast<const pldm_msg*>(response.data());
    reqHandler.handleResponse(eid, instanceId, 0, 0, responsePtr,
                              sizeof(response));

    // handleResponse() will free the instance ID after calling the response
    // handler, so the same instance ID is granted next as well
    ASSERT_EQ(validResponse, true);
    ASSERT_EQ(instanceId, dbusImplReq.getInstanceId(eid));
}

TEST_F(HandlerTest, singleRequestInstanceIdTimerExpired)
{
    Handler<NiceMock<MockRequest>> reqHandler(fd, event, dbusImplReq,
                                              seconds(1), 2, milliseconds(100));
    pldm::Request request{};
    auto instanceId = dbusImplReq.getInstanceId(eid);
    reqHandler.registerRequest(
        eid, instanceId, 0, 0, std::move(request),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));

    // Waiting for 500ms so that the instance ID expiry callback is invoked
    waitEventExpiry(milliseconds(500));

    // cleanup() will free the instance ID after calling the response
    // handler will no response, so the same instance ID is granted next
    ASSERT_EQ(instanceId, dbusImplReq.getInstanceId(eid));
    ASSERT_EQ(nullResponse, true);
}

TEST_F(HandlerTest, multipleRequestResponseScenario)
{
    Handler<NiceMock<MockRequest>> reqHandler(fd, event, dbusImplReq,
                                              seconds(2), 2, milliseconds(100));
    pldm::Request request{};
    auto instanceId = dbusImplReq.getInstanceId(eid);
    reqHandler.registerRequest(
        eid, instanceId, 0, 0, std::move(request),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));

    pldm::Request requestNxt{};
    auto instanceIdNxt = dbusImplReq.getInstanceId(eid);
    reqHandler.registerRequest(
        eid, instanceIdNxt, 0, 0, std::move(requestNxt),
        std::move(std::bind_front(&HandlerTest::pldmResponseCallBack, this)));

    pldm::Response response(sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    auto responsePtr = reinterpret_cast<const pldm_msg*>(response.data());
    reqHandler.handleResponse(eid, instanceIdNxt, 0, 0, responsePtr,
                              sizeof(response));
    ASSERT_EQ(validResponse, true);
    ASSERT_EQ(callbackCount, 1);
    validResponse = false;

    // Waiting for 500ms and handle the response for the first request, to
    // simulate a delayed response for the first request
    waitEventExpiry(milliseconds(500));

    reqHandler.handleResponse(eid, instanceId, 0, 0, responsePtr,
                              sizeof(response));

    ASSERT_EQ(validResponse, true);
    ASSERT_EQ(callbackCount, 2);
    ASSERT_EQ(instanceId, dbusImplReq.getInstanceId(eid));
}