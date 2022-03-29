#include "fw-update/firmware_inventory.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::fw_update;
using namespace pldm::fw_update::fw_inventory;

using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::StrEq;

TEST(Entry, Basic)
{
    sdbusplus::SdBusMock sdbusMock;
    auto busMock = sdbusplus::get_mocked_new(&sdbusMock);

    const std::string objPath{"/xyz/openbmc_project/software/bmc"};
    const UUID uuid{"ad4c8360-c54c-11eb-8529-0242ac130003"};
    const std::string version{"MAJOR.MINOR.PATCH"};

    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(IsNull(), StrEq(objPath)))
        .Times(1);

    Entry entry(busMock, objPath, version);
}

TEST(Entry, BasicEntryCreateAssociation)
{
    sdbusplus::SdBusMock sdbusMock;
    auto busMock = sdbusplus::get_mocked_new(&sdbusMock);

    const std::string objPath{"/xyz/openbmc_project/software/bmc"};
    const UUID uuid{"ad4c8360-c54c-11eb-8529-0242ac130003"};
    const std::string version{"MAJOR.MINOR.PATCH"};
    const std::string devObjectPath{
        "/xyz/openbmc_project/inventory/system/bmc"};

    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(IsNull(), StrEq(objPath)))
        .Times(1);

    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(objPath),
                    StrEq("xyz.openbmc_project.Association.Definitions"),
                    NotNull()))
        .Times(2)
        .WillRepeatedly(
            Invoke([=](sd_bus*, const char*, const char*, const char** names) {
                EXPECT_STREQ("Associations", names[0]);
                return 0;
            }));

    Entry entry(busMock, objPath, version);
    entry.createInventoryAssociation(devObjectPath);
    entry.createUpdateableAssociation("/xyz/openbmc_project/software");
}

TEST(Manager, SingleMatch)
{
    sdbusplus::SdBusMock sdbusMock;
    auto busMock = sdbusplus::get_mocked_new(&sdbusMock);

    EID eid = 1;
    const std::string activeCompVersion1{"Comp1v2.0"};
    const std::string activeCompVersion2{"Comp2v3.0"};
    constexpr uint16_t compClassification1 = 10;
    constexpr uint16_t compIdentifier1 = 300;
    constexpr uint8_t compClassificationIndex1 = 20;
    constexpr uint16_t compClassification2 = 16;
    constexpr uint16_t compIdentifier2 = 301;
    constexpr uint8_t compClassificationIndex2 = 30;
    ComponentInfoMap componentInfoMap{
        {eid,
         {{std::make_pair(compClassification1, compIdentifier1),
           std::make_tuple(compClassificationIndex1, activeCompVersion1)},
          {std::make_pair(compClassification2, compIdentifier2),
           std::make_tuple(compClassificationIndex2, activeCompVersion2)}}}};

    const UUID uuid{"ad4c8360-c54c-11eb-8529-0242ac130003"};
    const std::string compName1{"CompName1"};
    FirmwareInventoryInfo fwInventoryInfo{
        {uuid, {{compIdentifier1, compName1}}}};
    const std::string objPath = "/xyz/openbmc_project/software/" + compName1;

    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(IsNull(), StrEq(objPath)))
        .Times(1);
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(objPath),
                    StrEq("xyz.openbmc_project.Association.Definitions"),
                    NotNull()))
        .Times(2)
        .WillRepeatedly(
            Invoke([=](sd_bus*, const char*, const char*, const char** names) {
                EXPECT_STREQ("Associations", names[0]);
                return 0;
            }));

    sdbusplus::message::object_path deviceObjPath{
        "/xyz/openbmc_project/inventory/chassis/bmc"};
    Manager manager(busMock, fwInventoryInfo, componentInfoMap);
    manager.createEntry(eid, uuid, deviceObjPath);
}

TEST(Manager, MulipleMatch)
{
    sdbusplus::SdBusMock sdbusMock;
    auto busMock = sdbusplus::get_mocked_new(&sdbusMock);

    // ComponentInfoMap
    EID eid1 = 1;
    EID eid2 = 2;
    const std::string activeCompVersion1{"Comp1v2.0"};
    const std::string activeCompVersion2{"Comp2v3.0"};
    const std::string activeCompVersion3{"Comp2v4.0"};
    constexpr uint16_t compClassification1 = 10;
    constexpr uint16_t compIdentifier1 = 300;
    constexpr uint8_t compClassificationIndex1 = 20;
    constexpr uint16_t compClassification2 = 16;
    constexpr uint16_t compIdentifier2 = 301;
    constexpr uint8_t compClassificationIndex2 = 30;
    constexpr uint16_t compClassification3 = 10;
    constexpr uint16_t compIdentifier3 = 302;
    constexpr uint8_t compClassificationIndex3 = 40;
    ComponentInfoMap componentInfoMap{
        {eid1,
         {{std::make_pair(compClassification1, compIdentifier1),
           std::make_tuple(compClassificationIndex1, activeCompVersion1)},
          {std::make_pair(compClassification2, compIdentifier2),
           std::make_tuple(compClassificationIndex2, activeCompVersion2)}}},
        {eid2,
         {{std::make_pair(compClassification3, compIdentifier3),
           std::make_tuple(compClassificationIndex3, activeCompVersion3)}}}};

    // FirmwareInventoryInfo
    const UUID uuid1{"ad4c8360-c54c-11eb-8529-0242ac130003"};
    const UUID uuid2{"ad4c8360-c54c-11eb-8529-0242ac130004"};
    const std::string compName1{"CompName1"};
    const std::string compName2{"CompName2"};
    const std::string compName3{"CompName3"};
    FirmwareInventoryInfo fwInventoryInfo{
        {uuid1, {{compIdentifier1, compName1}, {compIdentifier2, compName2}}},
        {uuid2, {{compIdentifier3, compName3}}}};
    const std::string objPath1 = "/xyz/openbmc_project/software/" + compName1;
    const std::string objPath2 = "/xyz/openbmc_project/software/" + compName2;
    const std::string objPath3 = "/xyz/openbmc_project/software/" + compName3;

    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(IsNull(), StrEq(objPath1)))
        .Times(1);
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(objPath1),
                    StrEq("xyz.openbmc_project.Association.Definitions"),
                    NotNull()))
        .Times(2)
        .WillRepeatedly(
            Invoke([=](sd_bus*, const char*, const char*, const char** names) {
                EXPECT_STREQ("Associations", names[0]);
                return 0;
            }));
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(IsNull(), StrEq(objPath2)))
        .Times(1);
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(objPath2),
                    StrEq("xyz.openbmc_project.Association.Definitions"),
                    NotNull()))
        .Times(2)
        .WillRepeatedly(
            Invoke([=](sd_bus*, const char*, const char*, const char** names) {
                EXPECT_STREQ("Associations", names[0]);
                return 0;
            }));
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(IsNull(), StrEq(objPath3)))
        .Times(1);
    EXPECT_CALL(sdbusMock,
                sd_bus_emit_properties_changed_strv(
                    IsNull(), StrEq(objPath3),
                    StrEq("xyz.openbmc_project.Association.Definitions"),
                    NotNull()))
        .Times(2)
        .WillRepeatedly(
            Invoke([=](sd_bus*, const char*, const char*, const char** names) {
                EXPECT_STREQ("Associations", names[0]);
                return 0;
            }));

    sdbusplus::message::object_path deviceObjPath{
        "/xyz/openbmc_project/inventory/chassis/bmc"};
    Manager manager(busMock, fwInventoryInfo, componentInfoMap);
    manager.createEntry(eid1, uuid1, deviceObjPath);
    manager.createEntry(eid2, uuid2, deviceObjPath);
}