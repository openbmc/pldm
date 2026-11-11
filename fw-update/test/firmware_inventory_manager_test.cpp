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
    EXPECT_CALL(mockHandler, getSubtree)
        .WillOnce(::testing::Return(pldm::utils::GetSubTreeResponse{}));

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

TEST(FirmwareInventoryManager, ConfiguredComponentNames)
{
    MockdBusHandler mockHandler;
    const std::string boardPath =
        "/xyz/openbmc_project/inventory/system/board/Mocked_Board";
    const std::string recordPath = boardPath + "/Device_0";
    const std::string namesInterface =
        "xyz.openbmc_project.Configuration.PLDMFirmwareComponents";
    EXPECT_CALL(mockHandler, getAncestors)
        .WillRepeatedly(::testing::Return(
            pldm::utils::GetAncestorsResponse{{boardPath, {}}}));
    EXPECT_CALL(mockHandler,
                getSubtree(boardPath, 1, std::vector{namesInterface}))
        .WillRepeatedly(::testing::Return(pldm::utils::GetSubTreeResponse{
            {recordPath,
             {{"xyz.openbmc_project.EntityManager",
               {"xyz.openbmc_project.Configuration.MCTPI2CTarget",
                namesInterface, namesInterface + ".FirmwareComponents0",
                namesInterface + ".FirmwareComponents1"}}}}}));
    EXPECT_CALL(mockHandler,
                getDbusPropertiesVariant(
                    ::testing::_, ::testing::StrEq(recordPath),
                    ::testing::StrEq(namesInterface + ".FirmwareComponents0")))
        .WillRepeatedly(::testing::Return(
            pldm::utils::PropertyMap{{"ComponentIdentifier", uint64_t{2}},
                                     {"Name", std::string{"Firmware_A"}}}));
    EXPECT_CALL(mockHandler,
                getDbusPropertiesVariant(
                    ::testing::_, ::testing::StrEq(recordPath),
                    ::testing::StrEq(namesInterface + ".FirmwareComponents1")))
        .WillRepeatedly(::testing::Return(
            pldm::utils::PropertyMap{{"ComponentIdentifier", uint64_t{4}},
                                     {"Name", std::string{"Firmware B"}}}));

    pldm::eid eid = 1;
    Configurations configurations;
    configurations[recordPath] = pldm::MctpInfo(eid, "uuid", "", 0, "Device_0");

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

    auto softwarePath = [&](CompIdentifier compIdentifier) {
        inventoryManager.createFirmwareEntry(
            {eid, compIdentifier},
            std::format("Device_0_Component_{}", compIdentifier), "1.0", {}, {},
            nullptr);
        return static_cast<FirmwareInventoryTestInstance*>(
                   inventoryManager.getSoftwareMap()
                       .at({eid, compIdentifier})
                       .get())
            ->getSoftwarePath();
    };

    EXPECT_TRUE(softwarePath(2).starts_with(
        "/xyz/openbmc_project/software/Firmware_A_"));
    EXPECT_TRUE(softwarePath(3).starts_with(
        "/xyz/openbmc_project/software/Mocked_Board_Device_0_Component_3_"));
    EXPECT_TRUE(softwarePath(4).starts_with(
        "/xyz/openbmc_project/software/Mocked_Board_Device_0_Component_4_"));
}
