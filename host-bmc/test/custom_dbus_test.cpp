#include "../custom_dbus.hpp"

#include <gtest/gtest.h>

using namespace pldm::dbus;
TEST(CustomDBus, LocationCode)
{
    std::string tmpPath = "/abc/def";
    std::string locationCode = "testLocationCode";
    std::string retLocationCode{};

    CustomDBus::getCustomDBus().setLocationCode(tmpPath, locationCode);
    retLocationCode = CustomDBus::getCustomDBus().getLocationCode(tmpPath);

    EXPECT_EQ(locationCode, retLocationCode);
}

TEST(CustomDBus, OperationalStatus)
{
    std::string tmpPath = "/abc/def";
    bool status = false;
    bool retStatus = false;

    CustomDBus::getCustomDBus().setOperationalStatus(tmpPath, status);
    retStatus = CustomDBus::getCustomDBus().getOperationalStatus(tmpPath);

    EXPECT_EQ(status, false);
    EXPECT_EQ(retStatus, false);

    status = true;
    CustomDBus::getCustomDBus().setOperationalStatus(tmpPath, status);
    retStatus = CustomDBus::getCustomDBus().getOperationalStatus(tmpPath);

    EXPECT_EQ(status, true);
    EXPECT_EQ(retStatus, true);
}