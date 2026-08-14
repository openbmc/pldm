#include "fw-update/aggregate_update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/firmware_update.h>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;

class UpdateManagerTest : public testing::Test
{
  protected:
    UpdateManagerTest() :
        event(sdeventplus::Event::get_default()), instanceIdDb(),
        reqHandler(nullptr, event, instanceIdDb, false, seconds(1), 2,
                   milliseconds(100)),
        updateManager(event, reqHandler, instanceIdDb, descriptorMap,
                      componentInfoMap)
    {}

    /** @brief A record whose only applicable component is component 0 */
    static pkg::FirmwareDeviceIDRecord record(pkg::Descriptors descriptors)
    {
        return {0, {0}, "VersionString", std::move(descriptors), {}};
    }

    static pkg::Descriptor vendorDefined(const std::string& title,
                                         std::vector<uint8_t> data)
    {
        return {PLDM_FWUP_VENDOR_DEFINED,
                pkg::VendorDefinedDescriptorInfo{title, std::move(data)}};
    }

    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> reqHandler;
    pkg::DescriptorMap descriptorMap{};
    pkg::ComponentInfoMap componentInfoMap{};
    AggregateUpdateManager updateManager;
};

TEST_F(UpdateManagerTest, associatePkgToDevicesMatchesRecordWithOneDescriptor)
{
    const pkg::Descriptor uuid{
        PLDM_FWUP_UUID,
        pkg::DescriptorData{0x16, 0x20, 0x23, 0xC9, 0x3E, 0xC5, 0x41, 0x15,
                            0x95, 0xF4, 0x48, 0x70, 0x1D, 0x49, 0xD6, 0x75}};

    descriptorMap[1] = pkg::Descriptors{uuid};

    const pkg::FirmwareDeviceIDRecords records{record(pkg::Descriptors{uuid})};
    TotalComponentUpdates total = 0;

    const auto infos =
        updateManager.associatePkgToDevices(records, descriptorMap, total);

    ASSERT_EQ(infos.size(), 1);
    EXPECT_EQ(infos[0].first, 1);
    EXPECT_EQ(infos[0].second, 0);
    EXPECT_EQ(total, 1);
}

TEST_F(UpdateManagerTest,
       associatePkgToDevicesMatchesVendorDefinedDescriptorsInAnyOrder)
{
    // Two vendor-defined descriptors share PLDM_FWUP_VENDOR_DEFINED, so a
    // std::multimap keeps them in insertion order rather than in value order.
    // The device and the record list the same pair in opposite orders, which
    // std::includes only handles if both sides are ordered by the whole entry.
    const auto titleA = vendorDefined("TitleA", {0xAA, 0xAA});
    const auto titleB = vendorDefined("TitleB", {0xBB, 0xBB});

    descriptorMap[1] = pkg::Descriptors{titleB, titleA};

    const pkg::FirmwareDeviceIDRecords records{
        record(pkg::Descriptors{titleA, titleB})};
    TotalComponentUpdates total = 0;

    const auto infos =
        updateManager.associatePkgToDevices(records, descriptorMap, total);

    ASSERT_EQ(infos.size(), 1);
    EXPECT_EQ(infos[0].first, 1);
}

TEST_F(UpdateManagerTest, associatePkgToDevicesRejectsAnUnknownDescriptor)
{
    const auto titleA = vendorDefined("TitleA", {0xAA, 0xAA});
    const auto titleB = vendorDefined("TitleB", {0xBB, 0xBB});

    // The device only carries TitleA, so a record asking for both must not
    // match it
    descriptorMap[1] = pkg::Descriptors{titleA};

    const pkg::FirmwareDeviceIDRecords records{
        record(pkg::Descriptors{titleA, titleB})};
    TotalComponentUpdates total = 0;

    const auto infos =
        updateManager.associatePkgToDevices(records, descriptorMap, total);

    EXPECT_TRUE(infos.empty());
    EXPECT_EQ(total, 0);
}
