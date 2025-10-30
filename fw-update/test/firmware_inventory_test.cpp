#include "fw-update/aggregate_update_manager.hpp"
#include "fw-update/firmware_inventory.hpp"
#include "test/test_instance_id.hpp"

#include <string>

#include <gtest/gtest.h>

using namespace pldm::fw_update;
using namespace std::chrono;

class FirmwareInventoryTest : public FirmwareInventory
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

TEST(FirmwareInventoryTest, ConstructorSetsProperties)
{
    SoftwareIdentifier softwareIdentifier{1, 100};
    std::string expectedSoftwarePath =
        "/xyz/openbmc_project/software/PLDM_Device_TestDevice_1234";
    std::string expectedSoftwareVersion = "2.3.4";
    std::string expectedEndpointPath =
        "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    Descriptors firmwareDescriptors;
    DescriptorMap firmwareDescriptorMap{};
    ComponentInfo firmwareComponentInfo;
    ComponentInfoMap firmwareComponentInfoMap{};
    SoftwareVersionPurpose expectedPurpose = SoftwareVersionPurpose::Unknown;

    Event event(sdeventplus::Event::get_default());
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> handler(
        nullptr, event, instanceIdDb, false, seconds(1), 2, milliseconds(100));

    AggregateUpdateManager updateManager(
        event, handler, instanceIdDb, firmwareDescriptorMap,
        firmwareComponentInfoMap);

    FirmwareInventoryTest inventory(softwareIdentifier, expectedSoftwarePath,
                                    expectedSoftwareVersion,
                                    expectedEndpointPath, expectedPurpose);

    EXPECT_EQ(inventory.getSoftwarePath(), expectedSoftwarePath);
    auto associationTuples = inventory.getAssociation().associations();
    ASSERT_FALSE(associationTuples.empty());
    EXPECT_EQ(std::get<2>(associationTuples[0]), expectedEndpointPath);
    EXPECT_EQ(inventory.getVersion().version(), expectedSoftwareVersion);
    EXPECT_EQ(inventory.getVersion().purpose(), expectedPurpose);
}
