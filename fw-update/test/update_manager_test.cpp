#include "fw-update/update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/firmware_update.h>

#include <array>
#include <fstream>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;

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
