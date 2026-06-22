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
        "pldm-test.service", "arg",
        [&callbackInvoked](bool /*success*/) { callbackInvoked = true; });

    // For non-empty path, execute() should return without invoking callback
    // (callback will be invoked later when dbus responds asynchronously)
    EXPECT_FALSE(callbackInvoked.load());
}

TEST(ConditionUnitNameTest, NonTemplateUnitIgnoresArgs)
{
    EXPECT_EQ(conditionUnitName("pldm-test.service", ""), "pldm-test.service");
    EXPECT_EQ(conditionUnitName("pldm-test.service", "boardName=board1"),
              "pldm-test.service");
    EXPECT_EQ(conditionUnitName("pldm-test.target", "boardName=board1"),
              "pldm-test.target");
}

TEST(ConditionUnitNameTest, TemplateUnitTakesArgsAsInstanceName)
{
    EXPECT_EQ(
        conditionUnitName("pldm-test@.service",
                          "boardName=board1,applyTime=Immediate"),
        "pldm-test@boardName\\x3dboard1\\x2capplyTime\\x3dImmediate.service");
    EXPECT_EQ(conditionUnitName("pldm-test@.target", "boardName=board1"),
              "pldm-test@boardName\\x3dboard1.target");
}

TEST(ConditionUnitNameTest, TemplateUnitWithoutArgsIsLeftUnchanged)
{
    EXPECT_EQ(conditionUnitName("pldm-test@.service", ""),
              "pldm-test@.service");
}

TEST(EscapeUnitInstanceTest, EscapesCharactersInvalidInUnitNames)
{
    // Alphanumerics, ':', '_' and a non-leading '.' are kept as they are
    EXPECT_EQ(escapeUnitInstance("board1_A2:3.4"), "board1_A2:3.4");
    // '/' maps to '-', and a literal '-' is escaped since it stands for '/'
    EXPECT_EQ(escapeUnitInstance("a/b"), "a-b");
    EXPECT_EQ(escapeUnitInstance("a-b"), "a\\x2db");
    EXPECT_EQ(escapeUnitInstance("a=b,c"), "a\\x3db\\x2cc");
    // A leading '.' is escaped as well
    EXPECT_EQ(escapeUnitInstance(".board"), "\\x2eboard");
}
