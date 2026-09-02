#include "common/types.hpp"
#include "fw-update/item_update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <fcntl.h>
#include <libpldm/firmware_update.h>
#include <unistd.h>

#include <array>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace std::chrono;

class ItemUpdateManagerTest : public testing::Test
{
  protected:
    ItemUpdateManagerTest() :
        event(sdeventplus::Event::get_default()),
        handler(nullptr, event, instanceIdDb, false, seconds(1), 2,
                milliseconds(100)),
        packageFd(open("./test_pkg", O_RDONLY))
    {
        // Descriptors matching the single device record in ./test_pkg
        descriptors = {{PLDM_FWUP_UUID,
                        std::vector<uint8_t>{0x16, 0x20, 0x23, 0xC9, 0x3E, 0xC5,
                                             0x41, 0x15, 0x95, 0xF4, 0x48, 0x70,
                                             0x1D, 0x49, 0xD6, 0x75}}};
        componentInfo = {{std::make_pair(10, 100), 1}};
    }

    ~ItemUpdateManagerTest() override
    {
        if (packageFd >= 0)
        {
            close(packageFd);
        }
    }

    // Fixture is a friend of ItemUpdateManager: expose progress to TEST_F
    static ActivationProgress* progressOf(ItemUpdateManager& manager)
    {
        return manager.activationProgress.get();
    }

    // StartUpdate defers the package processing to the event loop
    void startUpdate(ItemUpdateManager& manager)
    {
        ASSERT_NE(packageFd, -1);
        manager.startUpdate(sdbusplus::message::unix_fd{packageFd});
        event.run(std::nullopt);
    }

    // RequestFirmwareData for a 512-byte chunk at the given offset
    Response requestFwData(ItemUpdateManager& manager, uint32_t offset = 0)
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

    static constexpr mctp_eid_t eid = 1;
    static constexpr auto objPath = "/xyz/openbmc_project/software/test";
    sdeventplus::Event event;
    TestInstanceIdDb instanceIdDb;
    requester::Handler<requester::Request> handler;
    int packageFd;
    Descriptors descriptors;
    ComponentInfo componentInfo;
};

TEST_F(ItemUpdateManagerTest, activationProgressAdvancesPerChunk)
{
    ItemUpdateManager manager(eid, event, handler, instanceIdDb, objPath, "sw",
                              descriptors, componentInfo);
    startUpdate(manager);
    ASSERT_NE(progressOf(manager), nullptr);
    EXPECT_EQ(progressOf(manager)->progress(), 0);

    // The 1024-byte component in ./test_pkg transfers as two 512B chunks
    auto response = requestFwData(manager, 0);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    // First chunk must be published: floor(97 * 512 / 1024)
    EXPECT_EQ(progressOf(manager)->progress(), 48);

    response = requestFwData(manager, 512);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    // Transfer complete: capped at 97 until verify/apply/activate
    EXPECT_EQ(progressOf(manager)->progress(), 97);
}

TEST_F(ItemUpdateManagerTest, activationProgressRestartsAfterTeardown)
{
    ItemUpdateManager manager(eid, event, handler, instanceIdDb, objPath, "sw",
                              descriptors, componentInfo);
    startUpdate(manager);
    ASSERT_NE(progressOf(manager), nullptr);
    auto response = requestFwData(manager, 0);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    ASSERT_EQ(progressOf(manager)->progress(), 48);

    // Fail the update at 48%: completeUpdate() tears the update down
    manager.updateDeviceCompletion(eid, false);
    EXPECT_EQ(progressOf(manager), nullptr);

    // Reuse the manager for a second update of the same package
    startUpdate(manager);
    ASSERT_NE(progressOf(manager), nullptr);
    EXPECT_EQ(progressOf(manager)->progress(), 0);

    // The first chunk is at 48% again: a stale lastProgress would swallow it
    response = requestFwData(manager, 0);
    ASSERT_GT(response.size(), sizeof(pldm_msg));
    EXPECT_EQ(response[sizeof(pldm_msg_hdr)], PLDM_SUCCESS);
    EXPECT_EQ(progressOf(manager)->progress(), 48);
}
