#include "libpldmresponder/fru_parser.hpp"

#include <gtest/gtest.h>

TEST(FruParser, allScenarios)
{
    using namespace pldm::responder::fru_parser;

    // Malformed FRU JSON
    ASSERT_THROW(FruParser("./fru_jsons/malformed"), std::exception);
    FruParser parser{"./fru_jsons/good"};

    // Get an item with a single PLDM FRU record
    FruRecordInfos motherBoard{
        {1,
         1,
         {{"xyz.openbmc_project.Inventory.Decorator.Asset", "Model", "string",
           2},
          {"xyz.openbmc_project.Inventory.Decorator.Asset", "PartNumber",
           "string", 3},
          {"xyz.openbmc_project.Inventory.Decorator.Asset", "SerialNumber",
           "string", 4},
          {"xyz.openbmc_project.Inventory.Decorator.Asset", "Manufacturer",
           "string", 5},
          {"xyz.openbmc_project.Inventory.Item", "PrettyName", "string", 8},
          {"xyz.openbmc_project.Inventory.Decorator.AssetTag", "AssetTag",
           "string", 11},
          {"xyz.openbmc_project.Inventory.Decorator.Revision", "Version",
           "string", 10}}},

        {254,
         1,
         {{"com.ibm.ipzvpd.VSYS", "RT", "bytearray", 2},
          {"com.ibm.ipzvpd.VSYS", "BR", "bytearray", 3}}}};
    auto motherBoardInfos = parser.getRecordInfo(
        "xyz.openbmc_project.Inventory.Item.Board.Motherboard");
    ASSERT_EQ(motherBoardInfos.size(), 2);
    ASSERT_EQ(motherBoard == motherBoardInfos, true);

    FruRecordInfos bmc{
        {1,
         1,
         {{"xyz.openbmc_project.Inventory.Decorator.Asset", "Model", "string",
           2},
          {"xyz.openbmc_project.Inventory.Decorator.Asset", "PartNumber",
           "string", 3},
          {"xyz.openbmc_project.Inventory.Decorator.Asset", "SerialNumber",
           "string", 4},
          {"xyz.openbmc_project.Inventory.Decorator.Asset", "Manufacturer",
           "string", 5},
          {"xyz.openbmc_project.Inventory.Item", "PrettyName", "string", 8},
          {"xyz.openbmc_project.Inventory.Decorator.AssetTag", "AssetTag",
           "string", 11},
          {"xyz.openbmc_project.Inventory.Decorator.Revision", "Version",
           "string", 10}}},

        {254,
         1,
         {{"com.ibm.ipzvpd.VINI", "RT", "bytearray", 2},
          {"com.ibm.ipzvpd.VINI", "B3", "bytearray", 3}}}};
    auto bmcInfos =
        parser.getRecordInfo("xyz.openbmc_project.Inventory.Item.Bmc");
    ASSERT_EQ(bmcInfos.size(), 2);
    ASSERT_EQ(bmc == bmcInfos, true);

    // Get an item type with 2 PLDM FRU records
    auto boardInfos =
        parser.getRecordInfo("xyz.openbmc_project.Inventory.Item.Board");
    ASSERT_EQ(boardInfos.size(), 2);

    // D-Bus lookup info for FRU information
    DBusLookupInfo lookupInfo{
        "xyz.openbmc_project.Inventory.Manager",
        "/xyz/openbmc_project/inventory",
        {"xyz.openbmc_project.Inventory.Item.Chassis",
         "xyz.openbmc_project.Inventory.Item.Board",
         "xyz.openbmc_project.Inventory.Item.Board.Motherboard",
         "xyz.openbmc_project.Inventory.Item.Panel",
         "xyz.openbmc_project.Inventory.Item.PowerSupply",
         "xyz.openbmc_project.Inventory.Item.Vrm",
         "xyz.openbmc_project.Inventory.Item.Cpu",
         "xyz.openbmc_project.Inventory.Item.Bmc",
         "xyz.openbmc_project.Inventory.Item.Dimm",
         "xyz.openbmc_project.Inventory.Item.Tpm",
         "xyz.openbmc_project.Inventory.Item.System"}};


    auto dbusInfo = parser.inventoryLookup();
    ASSERT_EQ(dbusInfo == lookupInfo, true);

    // Search for an invalid item type
    ASSERT_THROW(
        parser.getRecordInfo("xyz.openbmc_project.Inventory.Item.DIMM"),
        std::exception);
}

