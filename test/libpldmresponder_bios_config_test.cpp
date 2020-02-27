#include "bios_utils.hpp"
#include "libpldmresponder/bios_config.hpp"
#include "mocked_bios.hpp"
#include "mocked_utils.hpp"

#include <memory>
#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::bios::utils;

class TestBIOSConfig : public ::testing::Test
{
  public:
    static void SetUpTestCase() // will execute once at the begining of all
                                // TestBIOSConfig objects
    {
        char tmpdir[] = "/tmp/BIOSTables.XXXXXX";
        tableDir = fs::path(mkdtemp(tmpdir));
    }

    static void TearDownTestCase() // will be executed once at th end of all
                                   // TestBIOSConfig objects
    {
        fs::remove_all(tableDir);
    }

    static fs::path tableDir;
};

fs::path TestBIOSConfig::tableDir;

TEST_F(TestBIOSConfig, buildTablesTest)
{
    MockdBusHandler dbusHandler;

    BIOSConfig biosConfig("./bios_jsons", tableDir.c_str(), &dbusHandler);
    biosConfig.buildTables();

    auto stringTable = biosConfig.getBIOSTable(PLDM_BIOS_STRING_TABLE);
    auto attrTable = biosConfig.getBIOSTable(PLDM_BIOS_ATTR_TABLE);
    auto attrValueTable = biosConfig.getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);

    EXPECT_TRUE(stringTable);
    EXPECT_TRUE(attrTable);
    EXPECT_TRUE(attrValueTable);

    std::set<std::string> expectedStrings = {"HMCManagedState",
                                             "On",
                                             "Off",
                                             "FWBootSide",
                                             "Perm",
                                             "Temp",
                                             "InbandCodeUpdate",
                                             "Allowed",
                                             "NotAllowed",
                                             "CodeUpdatePolicy",
                                             "Concurrent",
                                             "Disruptive",
                                             "VDD_AVSBUS_RAIL",
                                             "SBE_IMAGE_MINIMUM_VALID_ECS",
                                             "INTEGER_INVALID_CASE",
                                             "str_example1",
                                             "str_example2",
                                             "str_example3"};
    std::set<std::string> strings;
    for (auto entry : BIOSTableIter<PLDM_BIOS_STRING_TABLE>(
             stringTable->data(), stringTable->size()))
    {
        auto str = table::string::decodeString(entry);
        strings.emplace(str);
    }

    EXPECT_EQ(strings, expectedStrings);
}