#include "fw-update/update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <cerrno>
#include <functional>
#include <sstream>
#include <utility>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;

namespace
{
bool failTimerArm = false;
}

extern "C" int __real_sd_event_source_set_time(sd_event_source*, uint64_t);
extern "C" int __wrap_sd_event_source_set_time(sd_event_source* source,
                                               uint64_t time)
{
    if (std::exchange(failTimerArm, false))
    {
        return -EIO;
    }
    return __real_sd_event_source_set_time(source, time);
}

namespace
{
class CompletionRecorder : public UpdateManagerBase
{
  public:
    using UpdateManagerBase::UpdateManagerBase;
    void updateDeviceCompletion(mctp_eid_t, bool success) override
    {
        results.push_back(success);
        if (onCompletion)
        {
            onCompletion();
        }
    }
    void updateActivationProgress() override {}
    void activatePackage() override {}
    void resetActivationState() override {}
    std::vector<bool> results;
    std::function<void()> onCompletion;
};

class DeviceUpdaterFlowTest : public testing::Test
{
  protected:
    DeviceUpdaterFlowTest() :
        event(sdeventplus::Event::get_default()),
        handler(nullptr, event, ids, false), manager(event, handler, ids),
        package(std::string(1024, '\0'))
    {
        handler.endpointMessageQueues[eid] =
            std::make_shared<requester::EndpointMessageQueue>(
                eid,
                std::deque<std::shared_ptr<requester::RegisteredRequest>>{},
                true);
        manager.setRequestedApplyTime(
            ApplyTimeIntf::RequestedApplyTimes::Immediate);
    }

    void create(uint16_t capabilities = 2, uint16_t requested = 0)
    {
        record = {1, {0}, "set", {}, {}};
        components = {{10, 100, 0xffffffff, 0, requested, 0, 1024, "new"}};
        info = {{{10, 100}, CompInfo{1, capabilities}}};
        updater = std::make_unique<DeviceUpdater>(
            eid, package, record, components, info, 512, &manager);
        acceptFirstComponent();
    }

    void createTwo(uint16_t firstCapabilities, uint16_t secondCapabilities,
                   uint16_t secondRequest = 0)
    {
        record = {1, {0, 1}, "set", {}, {}};
        components = {{10, 100, 0xffffffff, 0, 0, 0, 512, "new-a"},
                      {10, 200, 0xffffffff, 0, secondRequest, 512, 512,
                       "new-b"}};
        info = {{{10, 100}, CompInfo{1, firstCapabilities}},
                {{10, 200}, CompInfo{2, secondCapabilities}}};
        updater = std::make_unique<DeviceUpdater>(
            eid, package, record, components, info, 512, &manager);
        acceptFirstComponent();
    }

    void acceptFirstComponent()
    {
        // Enter the accepted-component state before simulating ApplyComplete.
        const Request response{
            0, PLDM_FWUP, PLDM_UPDATE_COMPONENT, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        updater->updateComponent(
            eid, reinterpret_cast<const pldm_msg*>(response.data()), 9);
    }

    void drain()
    {
        for (int i = 0; i < 8; ++i)
        {
            if (!event.run(microseconds(0)))
            {
                break;
            }
        }
    }

    auto& queue()
    {
        return handler.endpointMessageQueues.at(eid)->requestQueue;
    }

    void reply(uint8_t command, std::initializer_list<uint8_t> payload)
    {
        ASSERT_FALSE(queue().empty());
        auto request = queue().front();
        queue().pop_front();
        ASSERT_EQ(request->key.command, command);
        Request response(sizeof(pldm_msg_hdr), 0);
        response.insert(response.end(), payload);
        ids.free(eid, request->key.instanceId);
        request->responseHandler(
            eid, reinterpret_cast<const pldm_msg*>(response.data()),
            payload.size());
        drain();
    }

    void apply(uint8_t result = PLDM_FWUP_APPLY_SUCCESS, uint16_t methods = 0)
    {
        const Request request{0,
                              PLDM_FWUP,
                              PLDM_APPLY_COMPLETE,
                              result,
                              static_cast<uint8_t>(methods),
                              static_cast<uint8_t>(methods >> 8)};
        auto response = updater->applyComplete(
            reinterpret_cast<const pldm_msg*>(request.data()), 3);
        ASSERT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
        drain();
    }

    void ready()
    {
        reply(PLDM_GET_STATUS, {0, 2, 5, 3, 0, 100, 0, 0, 0, 0, 0});
    }

    uint8_t activationFlag()
    {
        EXPECT_FALSE(queue().empty());
        if (queue().empty())
        {
            return 0xff;
        }
        EXPECT_EQ(queue().front()->key.command, PLDM_ACTIVATE_FIRMWARE);
        return queue().front()->reqMsg[sizeof(pldm_msg_hdr)];
    }

    static constexpr uint8_t eid = 13;
    sdeventplus::Event event;
    TestInstanceIdDb ids;
    requester::Handler<requester::Request> handler;
    CompletionRecorder manager;
    std::stringstream package;
    FirmwareDeviceIDRecord record;
    ComponentImageInfos components;
    ComponentInfo info;
    std::unique_ptr<DeviceUpdater> updater;
};

TEST_F(DeviceUpdaterFlowTest, UnchangedCapabilityRequestsImmediateActivation)
{
    create();
    apply();
    ready();
    EXPECT_EQ(activationFlag(), 1);
}

TEST_F(DeviceUpdaterFlowTest, ApplyCompleteReplacesInventoryCapability)
{
    create(8);
    apply(PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD, 2);
    ready();
    EXPECT_EQ(activationFlag(), 1);
}

TEST_F(DeviceUpdaterFlowTest, OnResetDoesNotRequestUnrequestedActivation)
{
    create();
    manager.setRequestedApplyTime(ApplyTimeIntf::RequestedApplyTimes::OnReset);
    apply(PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD, 2);
    ready();
    EXPECT_EQ(activationFlag(), 0);
}

TEST_F(DeviceUpdaterFlowTest, MissingPostApplyStatusFailsUpdate)
{
    create();
    apply();
    ASSERT_FALSE(queue().empty());
    auto request = queue().front();
    queue().pop_front();
    ids.free(eid, request->key.instanceId);
    request->responseHandler(eid, nullptr, 0);
    drain();
    EXPECT_TRUE(queue().empty());
    ASSERT_EQ(manager.results.size(), 1);
    EXPECT_FALSE(manager.results.front());
}

TEST_F(DeviceUpdaterFlowTest, ActivationSilenceHonorsTheAdvertisedWindow)
{
    create();
    updater->selfContainedActivationRequested = true;
    const Request response{0, PLDM_FWUP, PLDM_ACTIVATE_FIRMWARE, 0, 60, 0};
    updater->activateFirmware(
        eid, reinterpret_cast<const pldm_msg*>(response.data()), 3);
    updater->pollActivationStatus();
    ASSERT_FALSE(queue().empty());
    auto request = queue().front();
    queue().pop_front();
    ids.free(eid, request->key.instanceId);
    request->responseHandler(eid, nullptr, 0);
    drain();
    EXPECT_TRUE(manager.results.empty());
    EXPECT_TRUE(updater->activationPollTimer->isRunning());
}

TEST_F(DeviceUpdaterFlowTest, InconsistentActivationStatusCannotSucceed)
{
    create();
    pldm_get_status_resp status{};
    status.current_state = PLDM_FD_STATE_IDLE;
    status.aux_state = PLDM_FD_IDLE_LEARN_COMPONENTS_READ_XFER;
    status.aux_state_status = PLDM_FD_SELF_CONTAINED_ACTIVATION_FAILURE;
    updater->handleActivationStatus(status);
    drain();
    ASSERT_EQ(manager.results.size(), 1);
    EXPECT_FALSE(manager.results.front());
}

TEST_F(DeviceUpdaterFlowTest, DestroyingUpdaterRetiresQueuedStatusRequest)
{
    create();
    apply();
    ASSERT_EQ(queue().size(), 1);
    updater.reset();
    EXPECT_TRUE(queue().empty());
    EXPECT_TRUE(manager.results.empty());
}

TEST_F(DeviceUpdaterFlowTest, AutomaticComponentDoesNotVetoLiveActivation)
{
    createTwo(2, 1);
    apply();
    ready();
    reply(PLDM_UPDATE_COMPONENT, {0, 0, 0, 0, 0, 0, 0, 0, 0});
    apply();
    ready();
    EXPECT_EQ(activationFlag(), 1);
}

TEST_F(DeviceUpdaterFlowTest, SkippedComponentCannotRequestOnResetActivation)
{
    createTwo(2, 2, 2);
    manager.setRequestedApplyTime(ApplyTimeIntf::RequestedApplyTimes::OnReset);
    apply(PLDM_FWUP_APPLY_SUCCESS_WITH_ACTIVATION_METHOD, 2);
    ready();
    reply(PLDM_UPDATE_COMPONENT, {0, 1, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(activationFlag(), 0);
}

TEST_F(DeviceUpdaterFlowTest, SkippedComponentCannotVetoImmediateActivation)
{
    createTwo(2, 8);
    apply();
    ready();
    reply(PLDM_UPDATE_COMPONENT, {0, 1, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(activationFlag(), 1);
}

TEST_F(DeviceUpdaterFlowTest, ActivationDeadlineEndsRepeatedSilence)
{
    create();
    updater->startSelfContainedActivationPolling(60);
    updater->activationDeadline = steady_clock::now() - seconds(1);
    updater->pollActivationStatus();
    ASSERT_FALSE(queue().empty());
    auto request = queue().front();
    queue().pop_front();
    ids.free(eid, request->key.instanceId);
    request->responseHandler(eid, nullptr, 0);
    drain();
    ASSERT_EQ(manager.results.size(), 1);
    EXPECT_FALSE(manager.results.front());
}

TEST_F(DeviceUpdaterFlowTest, InvalidPostApplyAuxStatusDoesNotAdvance)
{
    create();
    apply();
    manager.onCompletion = [this] { updater.reset(); };
    reply(PLDM_GET_STATUS, {0, 2, 5, 3, 0x0a, 100, 0, 0, 0, 0, 0});
    EXPECT_TRUE(queue().empty());
    EXPECT_EQ(updater, nullptr);
    ASSERT_EQ(manager.results.size(), 1);
    EXPECT_FALSE(manager.results.front());
}

TEST_F(DeviceUpdaterFlowTest, TimerArmFailureCompletesWithoutThrowing)
{
    create();
    failTimerArm = true;
    EXPECT_NO_THROW(updater->startSelfContainedActivationPolling(60));
    drain();
    ASSERT_EQ(manager.results.size(), 1);
    EXPECT_FALSE(manager.results.front());
}

TEST_F(DeviceUpdaterFlowTest, DeadlineRetiresAQueuedStatusRequest)
{
    create();
    updater->startSelfContainedActivationPolling(60);
    updater->pollActivationStatus();
    ASSERT_EQ(queue().size(), 1);
    updater->activationDeadlineTimer->start(microseconds(0), false);
    event.run(milliseconds(50));
    drain();
    ASSERT_EQ(manager.results.size(), 1);
    EXPECT_FALSE(manager.results.front());
    EXPECT_TRUE(queue().empty());
}

TEST_F(DeviceUpdaterFlowTest, IdleCancellationAndTimeoutReasonsCannotSucceed)
{
    for (uint8_t reason : {2, 3, 4, 5, 6, 7, 0x80})
    {
        SCOPED_TRACE(reason);
        create();
        manager.results.clear();
        updater->startSelfContainedActivationPolling(60);
        pldm_get_status_resp status{};
        status.current_state = PLDM_FD_STATE_IDLE;
        status.aux_state = PLDM_FD_IDLE_LEARN_COMPONENTS_READ_XFER;
        status.reason_code = reason;
        updater->handleActivationStatus(status);
        drain();
        ASSERT_EQ(manager.results.size(), 1);
        EXPECT_FALSE(manager.results.front());
    }
}

TEST_F(DeviceUpdaterFlowTest, IdleActivationAndInitializationReasonsSucceed)
{
    for (uint8_t reason : {0, 1})
    {
        SCOPED_TRACE(reason);
        create();
        manager.results.clear();
        updater->startSelfContainedActivationPolling(60);
        pldm_get_status_resp status{};
        status.current_state = PLDM_FD_STATE_IDLE;
        status.aux_state = PLDM_FD_IDLE_LEARN_COMPONENTS_READ_XFER;
        status.reason_code = reason;
        updater->handleActivationStatus(status);
        drain();
        ASSERT_EQ(manager.results.size(), 1);
        EXPECT_TRUE(manager.results.front());
    }
}
} // namespace

TEST_F(DeviceUpdaterFlowTest, PollingKeepsTheOutstandingStatusRequest)
{
    create();
    updater->startSelfContainedActivationPolling(60);
    updater->pollActivationStatus();
    updater->pollActivationStatus();
    ASSERT_EQ(queue().size(), 1u);
    updater->activationDeadlineTimer->start(microseconds(0), false);
    event.run(milliseconds(50));
    drain();
    EXPECT_TRUE(queue().empty());
    ASSERT_EQ(manager.results.size(), 1u);
    EXPECT_FALSE(manager.results.front());
}

TEST_F(DeviceUpdaterFlowTest, PendingCompletionIgnoresLateStatus)
{
    create();
    updater->startSelfContainedActivationPolling(60);
    updater->pollActivationStatus();
    updater->completeUpdate(false);
    reply(PLDM_GET_STATUS, {0, 6, 5, 0, 0, 50, 0, 0, 0, 0, 0});
    EXPECT_FALSE(updater->activationPollTimer->isRunning());
    EXPECT_TRUE(queue().empty());
    ASSERT_EQ(manager.results.size(), 1);
    EXPECT_FALSE(manager.results.front());
}
