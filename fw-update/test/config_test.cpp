#include "common/types.hpp"
#include "fw-update/config.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::fw_update;

TEST(ParseConfig, SingleEntry)
{

    DeviceInventoryInfo deviceInventoryInfo{
        {"ad4c8360-c54c-11eb-8529-0242ac130003",
         {"/xyz/openbmc_project/inventory/chassis/DeviceName",
          {{"parent", "child", "/xyz/openbmc_project/inventory/chassis"}}}}};

    FirmwareInventoryInfo fwInventoryInfo{
        {"ad4c8360-c54c-11eb-8529-0242ac130003", {{1, "ComponentName1"}}}};

    ComponentNameMapInfo componentNameMapInfo{
        {"ad4c8360-c54c-11eb-8529-0242ac130003",
         {{1, "ComponentName1"}, {2, "ComponentName2"}}}};

    DeviceInventoryInfo outdeviceInventoryInfo;
    FirmwareInventoryInfo outFwInventoryInfo;
    ComponentNameMapInfo outComponentNameMapConfig;

    parseConfig("./fw_update_jsons/fw_update_single_entry.json",
                outdeviceInventoryInfo, outFwInventoryInfo,
                outComponentNameMapConfig);

    EXPECT_EQ(outdeviceInventoryInfo, deviceInventoryInfo);
    EXPECT_EQ(outFwInventoryInfo, fwInventoryInfo);
    EXPECT_EQ(outComponentNameMapConfig, componentNameMapInfo);
}

TEST(ParseConfig, MultipleEntry)
{

    DeviceInventoryInfo deviceInventoryInfo{
        {"ad4c8360-c54c-11eb-8529-0242ac130003",
         {"/xyz/openbmc_project/inventory/chassis/DeviceName1",
          {{"parent", "child", "/xyz/openbmc_project/inventory/chassis"},
           {"right", "left", "/xyz/openbmc_project/inventory/direction"}}}},
        {"ad4c8360-c54c-11eb-8529-0242ac130004",
         {"/xyz/openbmc_project/inventory/chassis/DeviceName2", {}}}};

    FirmwareInventoryInfo fwInventoryInfo{
        {"ad4c8360-c54c-11eb-8529-0242ac130003", {{1, "ComponentName1"}}},
        {"ad4c8360-c54c-11eb-8529-0242ac130004",
         {{3, "ComponentName3"}, {4, "ComponentName4"}}}};

    ComponentNameMapInfo componentNameMapInfo{
        {"ad4c8360-c54c-11eb-8529-0242ac130003",
         {{1, "ComponentName1"}, {2, "ComponentName2"}}},
        {"ad4c8360-c54c-11eb-8529-0242ac130004",
         {{3, "ComponentName3"}, {4, "ComponentName4"}}}};

    DeviceInventoryInfo outdeviceInventoryInfo;
    FirmwareInventoryInfo outFwInventoryInfo;
    ComponentNameMapInfo outComponentNameMapConfig;

    parseConfig("./fw_update_jsons/fw_update_multiple_entry.json",
                outdeviceInventoryInfo, outFwInventoryInfo,
                outComponentNameMapConfig);

    EXPECT_EQ(outdeviceInventoryInfo, deviceInventoryInfo);
    EXPECT_EQ(outFwInventoryInfo, fwInventoryInfo);
    EXPECT_EQ(outComponentNameMapConfig, componentNameMapInfo);
}

TEST(ParseConfig, LimitedEntry)
{
    DeviceInventoryInfo deviceInventoryInfo{};

    FirmwareInventoryInfo fwInventoryInfo{
        {"ad4c8360-c54c-11eb-8529-0242ac130003", {{1, "ComponentName1"}}}};

    ComponentNameMapInfo componentNameMapInfo{
        {"ad4c8360-c54c-11eb-8529-0242ac130003",
         {{1, "ComponentName1"}, {2, "ComponentName2"}}}};

    DeviceInventoryInfo outdeviceInventoryInfo;
    FirmwareInventoryInfo outFwInventoryInfo;
    ComponentNameMapInfo outComponentNameMapConfig;

    parseConfig("./fw_update_jsons/fw_update_limited_entry.json",
                outdeviceInventoryInfo, outFwInventoryInfo,
                outComponentNameMapConfig);

    EXPECT_EQ(outdeviceInventoryInfo, deviceInventoryInfo);
    EXPECT_EQ(outFwInventoryInfo, fwInventoryInfo);
    EXPECT_EQ(outComponentNameMapConfig, componentNameMapInfo);
}