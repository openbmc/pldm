#include "fw-update/manager.hpp"
#include "test/test_instance_id.hpp"

#include <endian.h>

#include <sstream>
#include <vector>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;

namespace
{

// These tests start at device completion. They exercise aggregate completion,
// its production inventory callback and decoded D-Bus publication, without
// claiming to exercise package transfer or apply-time policy.
class AggregateUpdateTest : public testing::Test
{
  protected:
    AggregateUpdateTest() :
        event(sdeventplus::Event::get_default()),
        handler(nullptr, event, ids, false, seconds(1), 0, milliseconds(100)),
        manager(&dbus, event, handler, ids)
    {}

    void SetUp() override
    {
        MctpInfos endpoints;
        for (mctp_eid_t eid : {22, 23, 24})
        {
            handler.endpointMessageQueues[eid] =
                std::make_shared<requester::EndpointMessageQueue>(
                    eid,
                    std::deque<std::shared_ptr<requester::RegisteredRequest>>{},
                    true);
            endpoints.emplace_back(eid, "", "", 0, std::nullopt);
        }
        manager.handleMctpEndpoints(endpoints);
        for (mctp_eid_t eid : {22, 23, 24})
        {
            ASSERT_NO_FATAL_FAILURE(respondRefresh(eid, "old", {}, false));
        }

        auto& software =
            manager.inventoryMgr.firmwareInventoryManager.softwareMap;
        for (const auto& [identifier, entry] : software)
        {
            published.emplace(identifier, entry.get());
            paths.emplace(identifier, entry->softwarePath);
            items.emplace(
                identifier,
                manager.updateManager.updateManagers.at(identifier).get());
        }
    }

    static Request parameters(const std::string& active,
                              const std::string& pending)
    {
        Request bytes{0, PLDM_FWUP, PLDM_GET_FIRMWARE_PARAMETERS};
        auto append = [&bytes](const void* data, size_t size) {
            const auto* begin = static_cast<const uint8_t*>(data);
            bytes.insert(bytes.end(), begin, begin + size);
        };
        pldm_get_firmware_parameters_resp response{};
        response.comp_count = htole16(2);
        response.active_comp_image_set_ver_str_type = PLDM_STR_TYPE_ASCII;
        response.active_comp_image_set_ver_str_len = 9;
        append(&response, sizeof(response));
        append("image-set", 9);
        for (uint16_t id : {2, 513})
        {
            const std::string version = id == 2 ? "unchanged" : active;
            const std::string staged = id == 513 ? pending : "";
            pldm_component_parameter_entry entry{};
            entry.comp_classification = htole16(PLDM_COMP_FIRMWARE);
            entry.comp_identifier = htole16(id);
            entry.active_comp_ver_str_type = PLDM_STR_TYPE_ASCII;
            entry.active_comp_ver_str_len = version.size();
            if (!staged.empty())
            {
                entry.pending_comp_ver_str_type = PLDM_STR_TYPE_ASCII;
                entry.pending_comp_ver_str_len = staged.size();
            }
            entry.comp_activation_methods.value = htole16(0x003a);
            append(&entry, sizeof(entry));
            append(version.data(), version.size());
            append(staged.data(), staged.size());
        }
        return bytes;
    }

    auto& queue(mctp_eid_t eid)
    {
        return handler.endpointMessageQueues.at(eid)->requestQueue;
    }

    void respondRefresh(mctp_eid_t eid, const std::string& active,
                        const std::string& pending = {},
                        bool queryDownstream = true)
    {
        std::vector<uint8_t> commands{PLDM_QUERY_DEVICE_IDENTIFIERS,
                                      PLDM_GET_FIRMWARE_PARAMETERS};
        if (queryDownstream)
        {
            commands.insert(commands.begin() + 1,
                            PLDM_QUERY_DOWNSTREAM_DEVICES);
        }
        for (uint8_t command : commands)
        {
            ASSERT_EQ(queue(eid).size(),
                      queryDownstream && command != PLDM_GET_FIRMWARE_PARAMETERS
                          ? 2u
                          : 1u);
            auto request = queue(eid).front();
            queue(eid).pop_front();
            ASSERT_EQ(request->key.command, command);
            Request response =
                command == PLDM_QUERY_DEVICE_IDENTIFIERS
                    ? Request{0,
                              PLDM_FWUP,
                              PLDM_QUERY_DEVICE_IDENTIFIERS,
                              0,
                              8,
                              0,
                              0,
                              0,
                              1,
                              PLDM_FWUP_IANA_ENTERPRISE_ID,
                              0,
                              4,
                              0,
                              0x47,
                              0x16,
                              0,
                              0}
                    : parameters(active, pending);
            if (command == PLDM_QUERY_DOWNSTREAM_DEVICES)
            {
                response = {0, PLDM_FWUP, command,
                            PLDM_ERROR_UNSUPPORTED_PLDM_CMD};
            }
            ids.free(eid, request->key.instanceId);
            request->responseHandler(
                eid, reinterpret_cast<const pldm_msg*>(response.data()),
                response.size() - sizeof(pldm_msg_hdr));
        }
        EXPECT_TRUE(queue(eid).empty());
    }

    void beginCompletion()
    {
        auto& update = manager.updateManager;
        update.updateInProgress = true;
        update.startTime = steady_clock::now();
        update.activation = std::make_unique<Activation>(
            utils::DBusHandler::getBus(),
            "/xyz/openbmc_project/software/aggregate_test",
            ActivationIntf::Activations::Ready, &update);
        update.activationProgress = std::make_unique<ActivationProgress>(
            utils::DBusHandler::getBus(),
            "/xyz/openbmc_project/software/aggregate_test");
        for (mctp_eid_t eid : {22, 23})
        {
            update.deviceUpdaterMap.emplace(
                eid, std::make_unique<DeviceUpdater>(
                         eid, package, record, images,
                         manager.componentInfoMap.at(eid), 1024, &update));
        }
    }

    FirmwareInventory& software(mctp_eid_t eid, uint16_t component = 513)
    {
        return *manager.inventoryMgr.firmwareInventoryManager.softwareMap.at(
            {eid, component});
    }

    void expectPreservedInventory()
    {
        const auto& softwareMap =
            manager.inventoryMgr.firmwareInventoryManager.softwareMap;
        ASSERT_EQ(softwareMap.size(), published.size());
        for (const auto& [identifier, entry] : softwareMap)
        {
            SCOPED_TRACE(testing::PrintToString(identifier));
            EXPECT_EQ(entry.get(), published.at(identifier));
            EXPECT_EQ(entry->softwarePath, paths.at(identifier));
            EXPECT_EQ(manager.updateManager.updateManagers.at(identifier).get(),
                      items.at(identifier));
        }
        for (mctp_eid_t eid : {22, 23, 24})
        {
            EXPECT_EQ(software(eid, 2).version.version(), "unchanged");
        }
        EXPECT_EQ(software(24).version.version(), "old");
        EXPECT_TRUE(queue(24).empty());
    }

    sdeventplus::Event event;
    TestInstanceIdDb ids;
    requester::Handler<requester::Request> handler;
    const utils::DBusHandler dbus;
    FirmwareDeviceIDRecord record{1, {0}, "set", {}, {}};
    ComponentImageInfos images{
        {PLDM_COMP_FIRMWARE, 2, 0xffffffff, 0, 0, 0, 1024, "new"}};
    std::stringstream package{std::string(1024, '\0')};
    Manager manager;
    std::map<SoftwareIdentifier, FirmwareInventory*> published;
    std::map<SoftwareIdentifier, std::string> paths;
    std::map<SoftwareIdentifier, ItemUpdateManager*> items;
};

TEST_F(AggregateUpdateTest, CompletionRefreshesParticipantsInPlace)
{
    beginCompletion();
    auto& update = manager.updateManager;
    update.updateDeviceCompletion(22, true);
    EXPECT_TRUE(update.updateInProgress);
    EXPECT_TRUE(queue(22).empty());
    EXPECT_TRUE(queue(23).empty());

    update.updateDeviceCompletion(23, true);
    EXPECT_FALSE(update.updateInProgress);
    EXPECT_EQ(update.activation->activation(),
              ActivationIntf::Activations::Active);
    ASSERT_NO_FATAL_FAILURE(respondRefresh(22, "new"));
    ASSERT_NO_FATAL_FAILURE(respondRefresh(23, "new"));
    EXPECT_EQ(software(22).version.version(), "new");
    EXPECT_EQ(software(23).version.version(), "new");
    expectPreservedInventory();
}

TEST_F(AggregateUpdateTest, RefreshPublishesActiveRatherThanPendingVersions)
{
    beginCompletion();
    manager.updateManager.updateDeviceCompletion(22, true);
    manager.updateManager.updateDeviceCompletion(23, true);
    ASSERT_NO_FATAL_FAILURE(respondRefresh(22, "old", "pending-new"));
    ASSERT_NO_FATAL_FAILURE(respondRefresh(23, "old", "pending-new"));
    EXPECT_EQ(software(22).version.version(), "old");
    EXPECT_EQ(software(23).version.version(), "old");
    expectPreservedInventory();
}

TEST_F(AggregateUpdateTest, PartialFailureRefreshesEveryParticipantOnce)
{
    beginCompletion();
    auto& update = manager.updateManager;
    update.updateDeviceCompletion(22, false);
    update.updateDeviceCompletion(22, false);
    EXPECT_TRUE(update.updateInProgress);
    EXPECT_TRUE(queue(22).empty());
    EXPECT_TRUE(queue(23).empty());

    update.updateDeviceCompletion(23, true);
    EXPECT_FALSE(update.updateInProgress);
    EXPECT_EQ(update.activation->activation(),
              ActivationIntf::Activations::Failed);
    ASSERT_NO_FATAL_FAILURE(respondRefresh(22, "old"));
    ASSERT_NO_FATAL_FAILURE(respondRefresh(23, "new"));
    EXPECT_EQ(software(22).version.version(), "old");
    EXPECT_EQ(software(23).version.version(), "new");
    expectPreservedInventory();

    update.updateDeviceCompletion(22, false);
    update.updateDeviceCompletion(23, true);
    update.completeUpdate(false);
    EXPECT_TRUE(queue(22).empty());
    EXPECT_TRUE(queue(23).empty());
    EXPECT_EQ(update.activation->activation(),
              ActivationIntf::Activations::Failed);
}

TEST_F(AggregateUpdateTest, DuplicateSuccessfulCompletionDoesNotRefreshAgain)
{
    beginCompletion();
    auto& update = manager.updateManager;
    update.updateDeviceCompletion(22, true);
    update.updateDeviceCompletion(23, true);
    ASSERT_NO_FATAL_FAILURE(respondRefresh(22, "new"));
    ASSERT_NO_FATAL_FAILURE(respondRefresh(23, "new"));

    update.updateDeviceCompletion(22, true);
    update.updateDeviceCompletion(23, true);
    update.completeUpdate(true);
    EXPECT_TRUE(queue(22).empty());
    EXPECT_TRUE(queue(23).empty());
    EXPECT_EQ(update.activation->activation(),
              ActivationIntf::Activations::Active);
}

} // namespace
