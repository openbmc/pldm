#include "common/test/mocked_utils.hpp"
#include "fw-update/firmware_inventory.hpp"
#include "fw-update/firmware_inventory_manager.hpp"
#include "requester/test/test_peer_id_policy.hpp"
#include "test/test_instance_id.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm;
using namespace std::chrono;
using namespace pldm::fw_update;

// Helper class for testing: inherits FirmwareInventory and exposes protected
class FirmwareInventoryTestInstance : public pldm::fw_update::FirmwareInventory
{
  public:
    using FirmwareInventory::FirmwareInventory;

    const std::string& getSoftwarePath() const
    {
        return this->softwarePath;
    }
    const SoftwareAssociationDefinitions& getAssociation() const
    {
        return this->association;
    }
    const SoftwareVersion& getVersion() const
    {
        return this->version;
    }
};

class FirmwareInventoryManagerTest : public FirmwareInventoryManager
{
  public:
    FirmwareInventoryManagerTest(const pldm::utils::DBusHandler* handler,
                                 const Configurations& config,
                                 AggregateUpdateManager& updateManager,
                                 const EndpointRegistry& registry) :
        FirmwareInventoryManager(handler, config, updateManager, registry)
    {}

    SoftwareMap& getSoftwareMap()
    {
        return softwareMap;
    }
};

TEST(GetBoardPath_WithMockHandler, ReturnsExpectedBoardPath)
{
    MockdBusHandler mockHandler;
    InventoryPath inventoryPath =
        "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    pldm::utils::GetAncestorsResponse fakeResponse = {{inventoryPath, {}}};
    EXPECT_CALL(mockHandler, getAncestors)
        .WillOnce(::testing::Return(fakeResponse));

    Configurations configurations;
    std::string boardInventoryPath =
        "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    PeerId endpointId = 1;
    pldm::UUID endpointUuid = "uuid";
    pldm::MctpMedium endpointMedium = "medium";
    pldm::NetworkId endpointNetId = 0;
    pldm::MctpInfoName endpointName = "BMC";
    pldm::MctpInfo endpointInfo = std::make_tuple(
        endpointId, endpointUuid, endpointMedium, endpointNetId, endpointName);
    configurations[boardInventoryPath] = endpointInfo;

    Event event(sdeventplus::Event::get_default());
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> handler(
        nullptr, event, instanceIdDb, false, seconds(1), 2, milliseconds(100));

    DescriptorMap descriptorMap{};
    ComponentInfoMap componentInfoMap{};

    EndpointRegistry registry(std::make_unique<LowestFreePeerIdPolicy>());
    AggregateUpdateManager updateManager(event, handler, instanceIdDb, registry,
                                         descriptorMap, componentInfoMap);

    auto peer = registry.acquire(toEndpoint(endpointInfo));
    ASSERT_TRUE(peer);

    FirmwareInventoryManagerTest inventoryManager(&mockHandler, configurations,
                                                  updateManager, registry);

    SoftwareIdentifier softwareIdentifier{*peer, 100};
    SoftwareName softwareName{"TestDevice"};
    std::string firmwareVersion{"1.0.0"};
    Descriptors firmwareDescriptors;
    ComponentInfo firmwareComponentInfo;

    inventoryManager.createFirmwareEntry(
        softwareIdentifier, softwareName, firmwareVersion, firmwareDescriptors,
        firmwareComponentInfo, nullptr);
    ASSERT_TRUE(inventoryManager.getSoftwareMap().contains(softwareIdentifier));

    auto inventoryIt =
        inventoryManager.getSoftwareMap().find(softwareIdentifier);
    ASSERT_NE(inventoryIt, inventoryManager.getSoftwareMap().end());
    const auto* inventory =
        static_cast<FirmwareInventoryTestInstance*>(inventoryIt->second.get());
    ASSERT_NE(inventory, nullptr);
    EXPECT_NE(inventory->getSoftwarePath().find(
                  "/xyz/openbmc_project/software/PLDM_Device_TestDevice_"),
              std::string::npos);
    EXPECT_EQ(inventory->getVersion().version(), firmwareVersion);
}

TEST(FirmwareInventoryManagerNetwork, BoardPathFollowsNetworkNotJustEid)
{
    MockdBusHandler mockHandler;
    const std::string pathNetworkOne =
        "/xyz/openbmc_project/inventory/system/board/BoardOne/NicOne";
    const std::string pathNetworkTwo =
        "/xyz/openbmc_project/inventory/system/board/BoardTwo/NicTwo";
    const std::string boardTwo =
        "/xyz/openbmc_project/inventory/system/board/BoardTwo";

    // The same EID on two networks
    Configurations configurations;
    configurations[pathNetworkOne] =
        MctpInfo(10, "uuid1", "", 1, std::optional<std::string>("NicOne"));
    configurations[pathNetworkTwo] =
        MctpInfo(10, "uuid2", "", 2, std::optional<std::string>("NicTwo"));

    // Only the configuration of Network 2 may be looked up
    EXPECT_CALL(mockHandler, getAncestors(pathNetworkTwo, ::testing::_))
        .WillOnce(::testing::Return(
            pldm::utils::GetAncestorsResponse{{boardTwo, {}}}));
    EXPECT_CALL(mockHandler, getAncestors(pathNetworkOne, ::testing::_))
        .Times(0);

    Event event(sdeventplus::Event::get_default());
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> handler(
        nullptr, event, instanceIdDb, false, seconds(1), 2, milliseconds(100));
    DescriptorMap descriptorMap{};
    ComponentInfoMap componentInfoMap{};
    EndpointRegistry registry(std::make_unique<LowestFreePeerIdPolicy>());
    AggregateUpdateManager updateManager(event, handler, instanceIdDb, registry,
                                         descriptorMap, componentInfoMap);

    ASSERT_TRUE(registry.acquire(Endpoint{1, 10}));
    auto peerTwo = registry.acquire(Endpoint{2, 10});
    ASSERT_TRUE(peerTwo);

    FirmwareInventoryManagerTest inventoryManager(&mockHandler, configurations,
                                                  updateManager, registry);
    SoftwareIdentifier softwareIdentifier{*peerTwo, 100};
    inventoryManager.createFirmwareEntry(softwareIdentifier, "Dev", "1.0.0", {},
                                         {}, nullptr);

    ASSERT_TRUE(inventoryManager.getSoftwareMap().contains(softwareIdentifier));
    const auto* inventory = static_cast<FirmwareInventoryTestInstance*>(
        inventoryManager.getSoftwareMap().at(softwareIdentifier).get());
    EXPECT_NE(inventory->getSoftwarePath().find("BoardTwo_Dev_"),
              std::string::npos);
}
