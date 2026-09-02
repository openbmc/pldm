#include "fw-update/update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <functional>
#include <sstream>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;

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
    }

    void create()
    {
        record = {1, {0}, "set", {}, {}};
        components = {{10, 100, 0xffffffff, 0, 0, 0, 1024, "new"}};
        info = {{{10, 100}, 1}};
        updater = std::make_unique<DeviceUpdater>(
            eid, package, record, components, info, 512, &manager);
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

TEST_F(DeviceUpdaterFlowTest, ReadyAfterApplyStartsActivation)
{
    create();
    apply();
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

TEST_F(DeviceUpdaterFlowTest, DestroyedUpdaterIgnoresStatusResponse)
{
    create();
    apply();
    ASSERT_EQ(queue().size(), 1);
    auto request = queue().front();
    queue().pop_front();
    ids.free(eid, request->key.instanceId);
    updater.reset();
    request->responseHandler(eid, nullptr, 0);
    drain();
    EXPECT_TRUE(manager.results.empty());
}

TEST_F(DeviceUpdaterFlowTest, IdleAfterApplyFailsWithoutCancellation)
{
    create();
    apply();
    manager.onCompletion = [this] { updater.reset(); };
    reply(PLDM_GET_STATUS, {0, 0, 5, 3, 0, 100, 0, 0, 0, 0, 0});
    EXPECT_EQ(updater, nullptr);
    EXPECT_TRUE(queue().empty());
    ASSERT_EQ(manager.results.size(), 1);
    EXPECT_FALSE(manager.results.front());
}

} // namespace
