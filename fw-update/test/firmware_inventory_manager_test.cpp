#include "common/test/mocked_utils.hpp"
#include "fw-update/firmware_inventory.hpp"
#include "fw-update/firmware_inventory_manager.hpp"
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
                                 AggregateUpdateManager& updateManager) :
        FirmwareInventoryManager(handler, config, updateManager)
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
    pldm::eid endpointId = 1;
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

    AggregateUpdateManager updateManager(event, handler, instanceIdDb,
                                         descriptorMap, componentInfoMap);

    FirmwareInventoryManagerTest inventoryManager(&mockHandler, configurations,
                                                  updateManager);

    SoftwareIdentifier softwareIdentifier{endpointId, 100};
    SoftwareName softwareName{"TestDevice"};
    std::string firmwareVersion{"1.0.0"};
    Descriptors firmwareDescriptors;
    ComponentInfo firmwareComponentInfo;

    inventoryManager.createFirmwareEntry(
        softwareIdentifier, softwareName, firmwareVersion, firmwareDescriptors,
        firmwareComponentInfo);
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
