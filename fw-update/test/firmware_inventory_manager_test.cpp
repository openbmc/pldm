#include "fw-update/firmware_inventory.hpp"
#include "fw-update/firmware_inventory_manager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm;
using namespace std::chrono;
using namespace pldm::fw_update;

// Helper class for testing: inherits FirmwareInventory and exposes protected
class FirmwareInventoryTest : public pldm::fw_update::FirmwareInventory
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

class BoardPathProviderTest : public BoardPathProvider
{
  public:
    std::optional<std::filesystem::path> getBoardPath(const InventoryPath&)
    {
        return "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    }
};

class FirmwareInventoryManagerTest : public FirmwareInventoryManager
{
  public:
    FirmwareInventoryManagerTest(const Configurations& config) :
        FirmwareInventoryManager(config)
    {}

    FirmwareInventoryManagerTest(const Configurations& config,
                                 BoardPathProvider* provider) :
        FirmwareInventoryManager(config, provider)
    {}

    SoftwareMap& getSoftwareMap()
    {
        return softwareMap;
    }
    long int callGenerateSwId()
    {
        return generateSwId();
    }
};

TEST(DeleteFirmwareEntry_RemovesEntry, goodtest)
{
    // Initialize Configurations so createFirmwareEntry can succeed
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

    BoardPathProviderTest boardPathProviderMock;
    FirmwareInventoryManagerTest inventoryManager(configurations,
                                                  &boardPathProviderMock);

    SoftwareIdentifier softwareIdentifier{endpointId, 100};
    SoftwareName softwareName{"TestDevice"};
    std::string firmwareVersion{"1.0.0"};
    Descriptors firmwareDescriptors;
    ComponentInfo firmwareComponentInfo;

    inventoryManager.createFirmwareEntry(
        softwareIdentifier, softwareName, firmwareVersion, firmwareDescriptors,
        firmwareComponentInfo);
    ASSERT_TRUE(inventoryManager.getSoftwareMap().contains(softwareIdentifier));

    // Check FirmwareInventory object content
    auto inventoryIt =
        inventoryManager.getSoftwareMap().find(softwareIdentifier);
    ASSERT_NE(inventoryIt, inventoryManager.getSoftwareMap().end());
    // Cast to FirmwareInventoryTest to access protected getter
    const auto* inventory =
        static_cast<FirmwareInventoryTest*>(inventoryIt->second.get());
    ASSERT_NE(inventory, nullptr);
    // Check softwarePath prefix
    EXPECT_NE(inventory->getSoftwarePath().find(
                  "/xyz/openbmc_project/software/PLDM_Device_TestDevice_"),
              std::string::npos);

    // Check association content
    auto associationTuples = inventory->getAssociation().associations();
    ASSERT_FALSE(associationTuples.empty());
    // Check if endpoint is correct
    EXPECT_EQ(std::get<2>(associationTuples[0]), boardInventoryPath);

    // Check version content
    EXPECT_EQ(inventory->getVersion().version(), firmwareVersion);
    EXPECT_EQ(inventory->getVersion().purpose(),
              SoftwareVersionPurpose::Unknown);

    inventoryManager.deleteFirmwareEntry(softwareIdentifier.first);
    EXPECT_FALSE(
        inventoryManager.getSoftwareMap().contains(softwareIdentifier));
}

TEST(GenerateSwId_Range, Basic)
{
    Configurations config;
    FirmwareInventoryManagerTest manager(config);

    for (int i = 0; i < 10; ++i)
    {
        long id = manager.callGenerateSwId();
        EXPECT_GE(id, 0);
        EXPECT_LT(id, 10000);
    }
}
