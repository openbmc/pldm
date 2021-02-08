#include "oem/ibm/host-bmc/host_lamp_test.hpp"
#include "pldmd/dbus_impl_requester.hpp"

#include <string.h>

#include <gtest/gtest.h>

TEST(LampTest, Asserted)
{
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
    pldm::dbus_api::Requester dbusImplReq(bus, "/xyz/openbmc_project/pldm");
    pldm::led::HostLampTest lampTest(bus, "", 0, 0, dbusImplReq, nullptr);

    lampTest.asserted(false);
    EXPECT_EQ(lampTest.asserted(), false);
}