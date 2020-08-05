#include "libpldmresponder/fru_parser.hpp"

#include <gtest/gtest.h>

TEST(FruParser, allScenarios)
{
    using namespace pldm::responder::fru_parser;

    FruParser parser{"./fru_jsons/good"};

    // Get an item with a single PLDM FRU record
    FruRecordInfos cpu{{1,
                        1,
                        {{"xyz.openbmc_project.Inventory.Decorator.Asset",
                          "PartNumber", "string", 3},
                         {"xyz.openbmc_project.Inventory.Decorator.Asset",
                          "SerialNumber", "string", 4}}}};
    auto cpuInfos =
        parser.getRecordInfo("xyz.openbmc_project.Inventory.Item.Cpu");
    ASSERT_EQ(cpuInfos.size(), 2);
#if 0
    ASSERT_EQ(cpu == cpuInfos, true);
#endif

    // Get an item type with 2 PLDM FRU records
    auto boardInfos =
        parser.getRecordInfo("xyz.openbmc_project.Inventory.Item.Board");
    ASSERT_EQ(boardInfos.size(), 3);

#if 0
    // D-Bus lookup info for FRU information
    DBusLookupInfo lookupInfo{"xyz.openbmc_project.Inventory.Manager",
                              "/xyz/openbmc_project/inventory/system/",
                              {"xyz.openbmc_project.Inventory.Item.Board",
                               "xyz.openbmc_project.Inventory.Item.Cpu"}};

    auto dbusInfo = parser.inventoryLookup();
    ASSERT_EQ(dbusInfo == lookupInfo, true);
#endif

    // Search for an invalid item type
    ASSERT_THROW(
        parser.getRecordInfo("xyz.openbmc_project.Inventory.Item.DIMM"),
        std::exception);
}
