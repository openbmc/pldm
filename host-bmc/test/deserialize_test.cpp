#include "../dbus/deserialize.hpp"

#include <gtest/gtest.h>

using namespace std;
using namespace pldm::deserialize;

TEST(Deserialize, getEntityTypesTest)
{
    std::string filepath = "./../dummy_dbusConfig.json"

        auto pairData = getEntityTypes(filepath);
    auto restoretype = pairData->first;
    auto storetype = pairData->second;

    EXPECT_NE(restoretype.size(), 0);
    EXPECT_NE(storetype.size(), 0);
}
