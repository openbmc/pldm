#include "libpldmresponder/fru_parser.hpp"

#include <gtest/gtest.h>

TEST(InventoryLookup, allScenarios)
{
    using namespace fru_parser;

    FruRecordInfos board{{1,
                          1,
                          {{"xyz.openbmc_project.Inventory.Decorator.Asset",
                            "PartNumber", "string", 3},
                           {"xyz.openbmc_project.Inventory.Decorator.Asset",
                            "SerialNumber", "string", 4}}},
                         {254,
                          1,
                          {{"com.ibm.ipzvpd.VINI", "RT", "string", 2},
                           {"com.ibm.ipzvpd.VINI", "B3", "bytearray", 3}}}};

    FruRecordInfos cpu{{1,
                        1,
                        {{"xyz.openbmc_project.Inventory.Decorator.Asset",
                          "PartNumber", "string", 3},
                         {"xyz.openbmc_project.Inventory.Decorator.Asset",
                          "SerialNumber", "string", 4}}}};

    auto handle = fru_parser::get("./fru_jsons/good");

    auto boardInfos =
        handle.getRecordInfo("xyz.openbmc_project.Inventory.Item.Board");
    auto cpuInfos =
        handle.getRecordInfo("xyz.openbmc_project.Inventory.Item.Cpu");
    ASSERT_EQ(boardInfos.size(), 2);
    ASSERT_EQ(cpuInfos.size(), 1);
    ASSERT_EQ(cpu == cpuInfos, true);
    ASSERT_EQ(board == boardInfos, true);

    DBusLookupInfo lookupInfo{"xyz.openbmc_project.Inventory.Manager",
                              "/xyz/openbmc_project/inventory/system/",
                              {"xyz.openbmc_project.Inventory.Item.Board",
                               "xyz.openbmc_project.Inventory.Item.Cpu"}};

    auto dbusInfo = handle.inventoryLookup();
    ASSERT_EQ(dbusInfo == lookupInfo, true);
}
