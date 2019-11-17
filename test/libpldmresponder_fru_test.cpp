#include "libpldmresponder/fru_parser.hpp"

#include <gtest/gtest.h>

TEST(FruParser, allScenarios)
{
    using namespace fru_parser;

    // Empty directory condition
    ASSERT_THROW(fru_parser::get("./fru_json"), std::exception);
    // No master FRU JSON
    ASSERT_THROW(fru_parser::get("./fru_jsons/malformed1"), std::exception);

    auto handle = fru_parser::get("./fru_jsons/good");

    // Get an item with a single PLDM FRU record
    FruRecordInfos cpu{{1,
                        1,
                        {{"xyz.openbmc_project.Inventory.Decorator.Asset",
                          "PartNumber", "string", 3},
                         {"xyz.openbmc_project.Inventory.Decorator.Asset",
                          "SerialNumber", "string", 4}}}};
    auto cpuInfos =
        handle.getRecordInfo("xyz.openbmc_project.Inventory.Item.Cpu");
    ASSERT_EQ(cpuInfos.size(), 1);
    ASSERT_EQ(cpu == cpuInfos, true);

    // Get an item type with 2 PLDM FRU records
    auto boardInfos =
        handle.getRecordInfo("xyz.openbmc_project.Inventory.Item.Board");
    ASSERT_EQ(boardInfos.size(), 2);

    // D-Bus lookup info for FRU information
    DBusLookupInfo lookupInfo{"xyz.openbmc_project.Inventory.Manager",
                              "/xyz/openbmc_project/inventory/system/",
                              {"xyz.openbmc_project.Inventory.Item.Board",
                               "xyz.openbmc_project.Inventory.Item.Cpu"}};

    auto dbusInfo = handle.inventoryLookup();
    ASSERT_EQ(dbusInfo == lookupInfo, true);

    // Search for an invalid item type
    ASSERT_THROW(
        handle.getRecordInfo("xyz.openbmc_project.Inventory.Item.DIMM"),
        std::exception);
}
