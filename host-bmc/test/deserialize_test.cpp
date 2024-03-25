#include "../dbus/deserialize.hpp"

#include <utility>

#include <gtest/gtest.h>

TEST(Deserialize, getEntityTypesTest)
{
    std::string filepath = "dummy_dbusConfig.json";

    std::pair<std::set<uint16_t>, std::set<uint16_t>> pairData =
        pldm::deserialize::getEntityTypes(filepath);
    auto restoretype = pairData.first;
    auto storetype = pairData.second;

    EXPECT_NE(restoretype.size(), 0);
    EXPECT_NE(storetype.size(), 0);
}
