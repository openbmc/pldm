#include "fw-update/update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/firmware_update.h>
#include <unistd.h>

#include <array>
#include <filesystem>
#include <fstream>

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
                milliseconds(100)),
        package("./test_pkg", std::ios::binary | std::ios::in | std::ios::ate),
        packageSize(package.tellg()),
        updateManager(event, handler, instanceIdDb, descriptorMap,
                      componentInfoMap)
    {
        // Process the test package, which matches the device with eid, so
        // that the update is in progress.
        processPackage();
    }

    // Package processing is deferred to the event loop
    void processPackage()
    {
        updateManager.processStreamDefer(package, packageSize);
        event.run(std::nullopt);
    }

    // RequestFirmwareData for a 512-byte chunk at the given offset
    Response requestFwData(uint32_t offset = 0)
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
        return updateManager.handleRequest(
            eid, PLDM_REQUEST_FIRMWARE_DATA, requestMsg,
            sizeof(pldm_request_firmware_data_req));
    }

    // The package parser and the device updaters reading the package are gone
    bool packageReleased() const
    {
        return updateManager.deviceUpdaterMap.empty() && !updateManager.parser;
    }

    static constexpr mctp_eid_t eid = 1;
    const DescriptorMap descriptorMap{
        {eid,
         {{PLDM_FWUP_UUID,
           std::vector<uint8_t>{0x16, 0x20, 0x23, 0xC9, 0x3E, 0xC5, 0x41, 0x15,
                                0x95, 0xF4, 0x48, 0x70, 0x1D, 0x49, 0xD6,
                                0x75}}}}};
    const ComponentInfoMap componentInfoMap{
        {eid, {{std::make_pair(10, 100), 1}}}};
    Event event;
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> handler;
    std::ifstream package;
    uintmax_t packageSize;
    UpdateManager updateManager;
};

TEST_F(UpdateManagerTest, FailedUpdateReportsFullProgress)
{
    ASSERT_NE(updateManager.activationProgress, nullptr);
    EXPECT_EQ(updateManager.activationProgress->progress(), 0);

    updateManager.updateDeviceCompletion(eid, false);

    EXPECT_EQ(updateManager.activationProgress->progress(), 100);
    EXPECT_EQ(updateManager.activation->activation(),
              Activation::Activations::Failed);
}

TEST_F(UpdateManagerTest, SuccessfulUpdateReportsFullProgress)
{
    ASSERT_NE(updateManager.activationProgress, nullptr);

    updateManager.updateDeviceCompletion(eid, true);

    EXPECT_EQ(updateManager.activationProgress->progress(), 100);
    EXPECT_EQ(updateManager.activation->activation(),
              Activation::Activations::Active);
}

TEST_F(UpdateManagerTest, activationProgressAdvancesPerChunk)
{
    ASSERT_NE(updateManager.activationProgress, nullptr);
    EXPECT_EQ(updateManager.activationProgress->progress(), 0);

    // The 1024-byte component in ./test_pkg transfers as two 512B chunks
    auto response = requestFwData(0);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    // First chunk must be published: floor(97 * 512 / 1024)
    EXPECT_EQ(updateManager.activationProgress->progress(), 48);

    response = requestFwData(512);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    // Transfer complete: capped at 97 until verify/apply/activate
    EXPECT_EQ(updateManager.activationProgress->progress(), 97);
}

TEST_F(UpdateManagerTest, activationProgressRestartsAfterReset)
{
    ASSERT_NE(updateManager.activationProgress, nullptr);
    auto response = requestFwData(0);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    ASSERT_EQ(updateManager.activationProgress->progress(), 48);

    // Abandon the update at 48% and reuse the manager for another package
    updateManager.resetActivationState();
    EXPECT_EQ(updateManager.activationProgress, nullptr);
    processPackage();
    ASSERT_NE(updateManager.activationProgress, nullptr);
    EXPECT_EQ(updateManager.activationProgress->progress(), 0);

    // The first chunk is at 48% again: a stale lastProgress would swallow it
    response = requestFwData(0);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    EXPECT_EQ(updateManager.activationProgress->progress(), 48);
}

TEST_F(UpdateManagerTest, ReleasesPackageAfterCompletion)
{
    ASSERT_FALSE(packageReleased());

    updateManager.updateDeviceCompletion(eid, true);
    ASSERT_NE(updateManager.activation, nullptr);
    EXPECT_EQ(updateManager.activation->activation(),
              Activation::Activations::Active);
    // Released on the next loop iteration: the completing DeviceUpdater may
    // still be executing when the update completes
    EXPECT_FALSE(packageReleased());

    event.run(std::nullopt);
    EXPECT_TRUE(packageReleased());
    // The Software object stays so clients can read the final state
    ASSERT_NE(updateManager.activation, nullptr);
    EXPECT_EQ(updateManager.activation->activation(),
              Activation::Activations::Active);

    // A late FD command is refused instead of being served from a released
    // package
    auto response = requestFwData(0);
    ASSERT_EQ(response.size(), sizeof(pldm_msg_hdr) + sizeof(uint8_t));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_FWUP_COMMAND_NOT_EXPECTED);
}

TEST_F(UpdateManagerTest, StartsTheNextUpdateBeforeAPendingRelease)
{
    updateManager.updateDeviceCompletion(eid, true);
    ASSERT_FALSE(packageReleased());

    // The next StartUpdate is dispatched before the pending release runs
    updateManager.resetActivationState();
    processPackage();
    // Drain whatever else is pending: a stale release must not run now
    event.run(milliseconds(50));

    EXPECT_FALSE(packageReleased());
    ASSERT_NE(updateManager.activationProgress, nullptr);
    EXPECT_EQ(updateManager.activationProgress->progress(), 0);
}

// Discovery states where matched endpoints lack GetFirmwareParameters data
class UpdateManagerCompInfoTest : public testing::Test
{
  protected:
    UpdateManagerCompInfoTest() :
        event(sdeventplus::Event::get_default()),
        handler(nullptr, event, instanceIdDb, false, seconds(1), 2,
                milliseconds(100))
    {}

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

    // RequestFirmwareData for the first 512-byte chunk
    static Response requestFwData(UpdateManager& manager, mctp_eid_t eid)
    {
        std::array<uint8_t, sizeof(pldm_msg_hdr) +
                                sizeof(pldm_request_firmware_data_req)>
            reqFwDataReq{0x8A, 0x05, 0x15, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x02, 0x00, 0x00};
        auto requestMsg =
            reinterpret_cast<const pldm_msg*>(reqFwDataReq.data());
        return manager.handleRequest(eid, PLDM_REQUEST_FIRMWARE_DATA,
                                     requestMsg,
                                     sizeof(pldm_request_firmware_data_req));
    }

    // Descriptors matching the single device record in ./test_pkg
    const Descriptors uuidDescriptors{
        {PLDM_FWUP_UUID,
         std::vector<uint8_t>{0x16, 0x20, 0x23, 0xC9, 0x3E, 0xC5, 0x41, 0x15,
                              0x95, 0xF4, 0x48, 0x70, 0x1D, 0x49, 0xD6, 0x75}}};
    DescriptorMap descriptorMap;
    ComponentInfoMap componentInfoMap;
    Event event;
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> handler;
    std::filesystem::path packageCopy;
};

TEST_F(UpdateManagerCompInfoTest, skipDeviceMissingComponentInfo)
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

TEST_F(UpdateManagerCompInfoTest, skipDeviceEmptyComponentInfo)
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

TEST_F(UpdateManagerCompInfoTest, componentCountExcludesSkippedDevices)
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

TEST_F(UpdateManagerCompInfoTest, allDevicesMissingComponentInfoTerminates)
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

TEST_F(UpdateManagerCompInfoTest, skippedDeviceFailsUpdate)
{
    constexpr mctp_eid_t eidWithCompInfo = 1;
    constexpr mctp_eid_t eidWithoutCompInfo = 2;
    descriptorMap = {{eidWithCompInfo, uuidDescriptors},
                     {eidWithoutCompInfo, uuidDescriptors}};
    componentInfoMap = {{eidWithCompInfo, {{std::make_pair(10, 100), 1}}}};

    UpdateManager manager(event, handler, instanceIdDb, descriptorMap,
                          componentInfoMap);
    EXPECT_EQ(manager.processPackage(copyTestPackage()), 0);
    manager.updateDeviceCompletion(eidWithCompInfo, true);

    // The skipped device was not updated, so the update must not succeed
    ASSERT_NE(manager.activation, nullptr);
    EXPECT_EQ(manager.activation->activation(),
              software::Activation::Activations::Failed);
}
