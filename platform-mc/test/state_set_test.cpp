#include "platform-mc/state_set.hpp"

#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <gtest/gtest.h>

using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;

TEST(StateSets, shareInterface)
{
    pldm::platform_mc::StateSets stateSets(
        "/xyz/openbmc_project/inventory/system/Entity_1");

    auto& first = stateSets.getInterface<OperationalStatusIntf>();
    auto& second = stateSets.getInterface<OperationalStatusIntf>();
    EXPECT_EQ(&first, &second);
}

TEST(StateSets, initInterfaceOnce)
{
    pldm::platform_mc::StateSets stateSets(
        "/xyz/openbmc_project/inventory/system/Entity_2");

    auto& intf = stateSets.getInterface<OperationalStatusIntf>(
        [](OperationalStatusIntf& i) { i.functional(true); });
    EXPECT_TRUE(intf.functional());

    stateSets.getInterface<OperationalStatusIntf>([](OperationalStatusIntf& i) {
        i.functional(false);
    });
    EXPECT_TRUE(intf.functional());
}
