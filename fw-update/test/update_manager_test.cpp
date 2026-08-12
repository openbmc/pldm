#include "fw-update/update_manager.hpp"
#include "test/test_instance_id.hpp"

#include <libpldm/firmware_update.h>

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
        updateManager(event, handler, instanceIdDb, descriptorMap,
                      componentInfoMap)
    {
        // Process the test package, which matches the device with eid, so
        // that the update is in progress.
        uintmax_t packageSize = package.tellg();
        updateManager.processStreamDefer(package, packageSize);
        event.run(std::nullopt);
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
