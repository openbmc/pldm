#include "fw-update/systemd_interface.hpp"

#include <sdbusplus/bus.hpp>

#include <atomic>

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;

class SystemdInterfaceTest : public testing::Test
{
  protected:
    static sdbusplus::bus_t bus;
};

sdbusplus::bus_t SystemdInterfaceTest::bus = sdbusplus::bus::new_default();

TEST_F(SystemdInterfaceTest, ExecuteInvokesCallbackWhenConditionPathIsEmpty)
{
    auto& systemdInterface = SystemdInterface::getInstance(bus);

    bool callbackInvoked = false;
    bool callbackSuccess = false;

    systemdInterface.execute(
        "", // Empty path - should invoke callback immediately
        "", [&callbackInvoked, &callbackSuccess](bool success) {
            callbackInvoked = true;
            callbackSuccess = success;
        });

    // With empty condition path, callback should be invoked synchronously
    EXPECT_TRUE(callbackInvoked);
    EXPECT_TRUE(callbackSuccess);
}

TEST_F(SystemdInterfaceTest,
       ExecuteDoesNotInvokeCallbackSynchronouslyForNonEmptyPath)
{
    auto& systemdInterface = SystemdInterface::getInstance(bus);

    std::atomic_bool callbackInvoked{false};

    systemdInterface.execute(
        "pldm-test-service", "arg",
        [&callbackInvoked](bool /*success*/) { callbackInvoked = true; });

    // For non-empty path, execute() should return without invoking callback
    // (callback will be invoked later when dbus responds asynchronously)
    EXPECT_FALSE(callbackInvoked.load());
}
