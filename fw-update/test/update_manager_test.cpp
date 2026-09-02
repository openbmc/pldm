#include "common/types.hpp"
#include "fw-update/update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/firmware_update.h>
#include <unistd.h>

#include <array>
#include <filesystem>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;

namespace software = sdbusplus::xyz::openbmc_project::Software::server;

class UpdateManagerTest : public testing::Test
{
  protected:
    UpdateManagerTest() :
        event(sdeventplus::Event::get_default()),
        handler(nullptr, event, instanceIdDb, false, seconds(1), 2,
                milliseconds(100))
    {
        // Descriptors matching the single device record in ./test_pkg
        uuidDescriptors = {
            {PLDM_FWUP_UUID,
             std::vector<uint8_t>{0x16, 0x20, 0x23, 0xC9, 0x3E, 0xC5, 0x41,
                                  0x15, 0x95, 0xF4, 0x48, 0x70, 0x1D, 0x49,
                                  0xD6, 0x75}}};
    }

    // Fixture is a friend of UpdateManager: expose progress to TEST_F bodies
    static ActivationProgress* progressOf(UpdateManager& manager)
    {
        return manager.activationProgress.get();
    }

    void TearDown() override
    {
        if (!packageCopy.empty())
        {
            std::filesystem::remove(packageCopy);
        }
    }

    // processPackage() deletes the package on failure, so update a copy
    std::filesystem::path copyTestPackage()
    {
        char tmpl[] = "/tmp/pldm_test_pkg.XXXXXX";
        int fd = mkstemp(tmpl);
        EXPECT_NE(fd, -1);
        close(fd);
        packageCopy = tmpl;
        std::filesystem::copy_file(
            "./test_pkg", packageCopy,
            std::filesystem::copy_options::overwrite_existing);
        return packageCopy;
    }

    // RequestFirmwareData for a 512-byte chunk at the given offset
    Response requestFwData(UpdateManager& manager, mctp_eid_t eid,
                           uint32_t offset = 0)
    {
        constexpr uint32_t length = 512;
        std::array<uint8_t, sizeof(pldm_msg_hdr) +
                                sizeof(pldm_request_firmware_data_req)>
            reqFwDataReq{0x8A, 0x05, 0x15};
        uint8_t* payload = reqFwDataReq.data() + sizeof(pldm_msg_hdr);
        for (size_t i = 0; i < sizeof(offset); ++i)
        {
            payload[i] = (offset >> (8 * i)) & 0xFF;
            payload[i + sizeof(offset)] = (length >> (8 * i)) & 0xFF;
        }
        auto requestMsg =
            reinterpret_cast<const pldm_msg*>(reqFwDataReq.data());
        return manager.handleRequest(eid, PLDM_REQUEST_FIRMWARE_DATA,
                                     requestMsg,
                                     sizeof(pldm_request_firmware_data_req));
    }

    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> handler;
    Descriptors uuidDescriptors;
    DescriptorMap descriptorMap;
    ComponentInfoMap componentInfoMap;
    std::filesystem::path packageCopy;
};

TEST_F(UpdateManagerTest, activationProgressAdvancesPerChunk)
{
    constexpr mctp_eid_t eid = 1;
    descriptorMap = {{eid, uuidDescriptors}};
    componentInfoMap = {{eid, {{std::make_pair(10, 100), 1}}}};

    UpdateManager manager(event, handler, instanceIdDb, descriptorMap,
                          componentInfoMap);
    EXPECT_EQ(manager.processPackage(copyTestPackage()), 0);
    ASSERT_NE(progressOf(manager), nullptr);
    EXPECT_EQ(progressOf(manager)->progress(), 0);

    // The 1024-byte component in ./test_pkg transfers as two 512B chunks
    auto response = requestFwData(manager, eid, 0);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    // First chunk must be published: floor(97 * 512 / 1024)
    EXPECT_EQ(progressOf(manager)->progress(), 48);

    response = requestFwData(manager, eid, 512);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    // Transfer complete: capped at 97 until verify/apply/activate
    EXPECT_EQ(progressOf(manager)->progress(), 97);
}

TEST_F(UpdateManagerTest, skipDeviceMissingComponentInfo)
{
    constexpr mctp_eid_t eidWithCompInfo = 1;
    constexpr mctp_eid_t eidWithoutCompInfo = 2;
    descriptorMap = {{eidWithCompInfo, uuidDescriptors},
                     {eidWithoutCompInfo, uuidDescriptors}};
    componentInfoMap = {{eidWithCompInfo, {{std::make_pair(10, 100), 1}}}};

    UpdateManager manager(event, handler, instanceIdDb, descriptorMap,
                          componentInfoMap);
    EXPECT_EQ(manager.processPackage(copyTestPackage()), 0);

    // The device with component info is serviced by a DeviceUpdater
    auto response = requestFwData(manager, eidWithCompInfo);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);

    // The device without component info must not get a DeviceUpdater
    response = requestFwData(manager, eidWithoutCompInfo);
    ASSERT_EQ(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_FWUP_COMMAND_NOT_EXPECTED);
}

TEST_F(UpdateManagerTest, skipDeviceEmptyComponentInfo)
{
    constexpr mctp_eid_t eidWithCompInfo = 1;
    constexpr mctp_eid_t eidWithEmptyCompInfo = 2;
    descriptorMap = {{eidWithCompInfo, uuidDescriptors},
                     {eidWithEmptyCompInfo, uuidDescriptors}};
    // GetFirmwareParameters succeeded but reported zero components
    componentInfoMap = {{eidWithCompInfo, {{std::make_pair(10, 100), 1}}},
                        {eidWithEmptyCompInfo, {}}};

    UpdateManager manager(event, handler, instanceIdDb, descriptorMap,
                          componentInfoMap);
    EXPECT_EQ(manager.processPackage(copyTestPackage()), 0);

    // The device with component info is serviced by a DeviceUpdater
    auto response = requestFwData(manager, eidWithCompInfo);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);

    // The zero-component device must not get a DeviceUpdater
    response = requestFwData(manager, eidWithEmptyCompInfo);
    ASSERT_EQ(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_FWUP_COMMAND_NOT_EXPECTED);
}

TEST_F(UpdateManagerTest, componentCountExcludesSkippedDevices)
{
    constexpr mctp_eid_t eidWithCompInfo = 1;
    constexpr mctp_eid_t eidWithoutCompInfo = 2;
    constexpr mctp_eid_t eidWithEmptyCompInfo = 3;
    descriptorMap = {{eidWithCompInfo, uuidDescriptors},
                     {eidWithoutCompInfo, uuidDescriptors},
                     {eidWithEmptyCompInfo, uuidDescriptors}};
    componentInfoMap = {{eidWithCompInfo, {{std::make_pair(10, 100), 1}}},
                        {eidWithEmptyCompInfo, {}}};
    FirmwareDeviceIDRecords fwDeviceIDRecords{
        {1, {0x00}, "VersionString2", uuidDescriptors, {}}};

    UpdateManager manager(event, handler, instanceIdDb, descriptorMap,
                          componentInfoMap);
    TotalComponentUpdates totalNumComponentUpdates = 0;
    auto deviceUpdaterInfos = manager.associatePkgToDevices(
        fwDeviceIDRecords, descriptorMap, componentInfoMap,
        totalNumComponentUpdates);

    // Only the device with non-empty component info is matched and counted
    ASSERT_EQ(deviceUpdaterInfos.size(), 1);
    EXPECT_EQ(deviceUpdaterInfos[0].first, eidWithCompInfo);
    EXPECT_EQ(deviceUpdaterInfos[0].second, 0);
    EXPECT_EQ(totalNumComponentUpdates, 1);
}

TEST_F(UpdateManagerTest, allDevicesMissingComponentInfoTerminates)
{
    constexpr mctp_eid_t eidWithoutCompInfo = 2;
    descriptorMap = {{eidWithoutCompInfo, uuidDescriptors}};

    UpdateManager manager(event, handler, instanceIdDb, descriptorMap,
                          componentInfoMap);
    EXPECT_EQ(manager.processPackage(copyTestPackage()), -1);

    // The update must terminate as Invalid instead of wedging in Activating
    ASSERT_NE(manager.activation, nullptr);
    EXPECT_EQ(manager.activation->activation(),
              software::Activation::Activations::Invalid);
}
