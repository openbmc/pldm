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