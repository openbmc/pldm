#include "fw-update/inventory_manager.hpp"
#include "test/test_instance_id.hpp"

#include <endian.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;

// Only the transport boundary is simulated. Discovery, D-Bus inventory,
// StartUpdate and deferred package processing use the production classes.
class InventoryRefreshTest : public testing::Test
{
  protected:
    static constexpr mctp_eid_t eid = 22;

    InventoryRefreshTest() :
        event(sdeventplus::Event::get_default()),
        handler(nullptr, event, ids, false, seconds(1), 0, milliseconds(100)),
        updates(event, handler, ids, descriptors, components),
        inventory(&dbus, handler, ids, descriptors, downstream, components,
                  configurations, updates)
    {
        handler.endpointMessageQueues[eid] =
            std::make_shared<requester::EndpointMessageQueue>(
                eid,
                std::deque<std::shared_ptr<requester::RegisteredRequest>>{},
                true);
    }

    static Request identifiers(uint8_t lastByte = 0x75)
    {
        return {
            0,
            PLDM_FWUP,
            PLDM_QUERY_DEVICE_IDENTIFIERS,
            0,
            20,
            0,
            0,
            0,
            1,
            PLDM_FWUP_UUID,
            0,
            16,
            0,
            0x16,
            0x20,
            0x23,
            0xc9,
            0x3e,
            0xc5,
            0x41,
            0x15,
            0x95,
            0xf4,
            0x48,
            0x70,
            0x1d,
            0x49,
            0xd6,
            lastByte};
    }

    static Request parameters(
        const std::string& version = "old",
        const std::vector<uint16_t>& componentIds = {100, 200},
        const std::string& imageSet = "image-set", uint16_t methods = 0x003a)
    {
        Request bytes{0, PLDM_FWUP, PLDM_GET_FIRMWARE_PARAMETERS};
        auto append = [&bytes](const void* data, size_t size) {
            auto start = static_cast<const uint8_t*>(data);
            bytes.insert(bytes.end(), start, start + size);
        };
        pldm_get_firmware_parameters_resp response{};
        response.comp_count = htole16(componentIds.size());
        response.active_comp_image_set_ver_str_type = PLDM_STR_TYPE_ASCII;
        response.active_comp_image_set_ver_str_len = imageSet.size();
        append(&response, sizeof(response));
        append(imageSet.data(), imageSet.size());
        for (auto id : componentIds)
        {
            pldm_component_parameter_entry entry{};
            entry.comp_classification = htole16(PLDM_COMP_FIRMWARE);
            entry.comp_identifier = htole16(id);
            entry.active_comp_ver_str_type = PLDM_STR_TYPE_ASCII;
            entry.active_comp_ver_str_len = version.size();
            entry.comp_activation_methods.value = htole16(methods);
            append(&entry, sizeof(entry));
            append(version.data(), version.size());
        }
        return bytes;
    }

    void query(uint8_t lastByte = 0x75)
    {
        auto response = identifiers(lastByte);
        inventory.queryDeviceIdentifiers(
            eid, reinterpret_cast<const pldm_msg*>(response.data()),
            response.size() - sizeof(pldm_msg_hdr));
    }

    void respond(const Request& response)
    {
        auto& queue = handler.endpointMessageQueues.at(eid)->requestQueue;
        ASSERT_FALSE(queue.empty());
        auto request = queue.front();
        queue.pop_front();
        ASSERT_EQ(request->key.command, PLDM_GET_FIRMWARE_PARAMETERS);
        ids.free(eid, request->key.instanceId);
        request->responseHandler(
            eid,
            response.empty()
                ? nullptr
                : reinterpret_cast<const pldm_msg*>(response.data()),
            response.empty() ? 0 : response.size() - sizeof(pldm_msg_hdr));
    }

    void discover()
    {
        query();
        respond(parameters());
    }

    ItemUpdateManager* item(uint16_t id)
    {
        return updates.updateManagers.at({eid, id}).get();
    }

    FirmwareInventory* software(uint16_t id)
    {
        return inventory.firmwareInventoryManager.softwareMap.at({eid, id})
            .get();
    }

    void start(ItemUpdateManager& manager)
    {
        int fd = open("test_pkg", O_RDONLY);
        ASSERT_GE(fd, 0);
        manager.startUpdate(sdbusplus::message::unix_fd(fd),
                            ApplyTimeIntf::RequestedApplyTimes::Immediate);
        close(fd);
    }

    void processPackage(ItemUpdateManager& manager)
    {
        for (size_t i = 0; i < 10 && !manager.deviceUpdater; ++i)
        {
            ASSERT_GE(sd_event_run(event.get(), 0), 0);
        }
        ASSERT_NE(manager.deviceUpdater, nullptr);
    }

    const utils::DBusHandler dbus;
    sdeventplus::Event event;
    TestInstanceIdDb ids;
    requester::Handler<requester::Request> handler;
    DescriptorMap descriptors;
    DownstreamDescriptorMap downstream;
    ComponentInfoMap components;
    Configurations configurations;
    AggregateUpdateManager updates;
    InventoryManager inventory;
};

TEST_F(InventoryRefreshTest, CompleteCapabilitiesReachEveryItemUpdater)
{
    discover();
    const ComponentInfo expected{{{PLDM_COMP_FIRMWARE, 100}, {0, 0x003a}},
                                 {{PLDM_COMP_FIRMWARE, 200}, {0, 0x003a}}};
    for (uint16_t id : {0, 100, 200})
    {
        SCOPED_TRACE(id);
        auto& manager = *item(id);
        EXPECT_EQ(manager.componentInfo, expected);
        start(manager);
        processPackage(manager);
        ASSERT_NE(manager.deviceUpdater, nullptr);
        EXPECT_EQ(manager.deviceUpdater->compInfo, expected);
    }
}

TEST_F(InventoryRefreshTest, RefreshPreservesAnItemStartedBeforeItsResponse)
{
    discover();
    auto* manager = item(100);
    auto* published = software(100);
    const auto path = published->softwarePath;
    const auto* metadata = &manager->componentInfo;
    query();
    start(*manager);
    std::weak_ptr lifetime = manager->aliveToken;
    respond(parameters("new"));
    ASSERT_FALSE(lifetime.expired());
    EXPECT_EQ(item(100), manager);
    EXPECT_EQ(software(100), published);
    EXPECT_EQ(software(100)->softwarePath, path);
    EXPECT_EQ(&item(100)->componentInfo, metadata);
    EXPECT_TRUE(item(100)->updateInProgress);
    EXPECT_EQ(software(100)->version.version(), "new");
    processPackage(*manager);
}

TEST_F(InventoryRefreshTest, RefreshKeepsAnActiveUpdaterSnapshot)
{
    discover();
    auto* manager = item(100);
    query();
    start(*manager);
    processPackage(*manager);
    const auto* updater = manager->deviceUpdater.get();
    const auto snapshot = updater->compInfo;
    std::weak_ptr lifetime = manager->aliveToken;
    respond(parameters("new", {100, 200}, "new-set", 0x0008));
    ASSERT_FALSE(lifetime.expired());
    EXPECT_EQ(item(100)->deviceUpdater.get(), updater);
    EXPECT_EQ(updater->compInfo, snapshot);
    EXPECT_EQ(std::get<CompActivationMethods>(
                  item(100)->componentInfo.at({PLDM_COMP_FIRMWARE, 200})),
              0x0008);
}

TEST_F(InventoryRefreshTest, MalformedLaterEntryKeepsPublishedVersions)
{
    discover();
    auto* published = software(100);
    const auto oldComponents = components;
    query(0x76);
    auto malformed = parameters("new");
    malformed.pop_back();
    respond(malformed);
    EXPECT_EQ(components, oldComponents);
    EXPECT_EQ(software(100), published);
    EXPECT_EQ(software(100)->version.version(), "old");
    EXPECT_EQ(software(200)->version.version(), "old");
}

TEST_F(InventoryRefreshTest, MissingDeclaredEntryKeepsTheOldSnapshot)
{
    discover();
    const auto oldComponents = components;
    query();
    auto malformed = parameters("new");
    auto* header = reinterpret_cast<pldm_get_firmware_parameters_resp*>(
        malformed.data() + sizeof(pldm_msg_hdr));
    header->comp_count = htole16(3);
    respond(malformed);
    EXPECT_EQ(components, oldComponents);
    EXPECT_EQ(software(100)->version.version(), "old");
}

TEST_F(InventoryRefreshTest, TimeoutRetainsExistingInventory)
{
    discover();
    const auto oldDescriptors = descriptors;
    const auto oldComponents = components;
    query();
    respond({});
    EXPECT_EQ(descriptors, oldDescriptors);
    EXPECT_EQ(components, oldComponents);
    EXPECT_EQ(software(100)->version.version(), "old");
    EXPECT_TRUE(handler.endpointMessageQueues.at(eid)->requestQueue.empty());
}
