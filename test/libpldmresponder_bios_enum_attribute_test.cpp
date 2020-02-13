#include "libpldmresponder/bios_enum_attribute.hpp"
#include "mock_utils.hpp"

#include <memory>
#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using testing::ElementsAreArray;
using ::testing::Return;

class TestBIOSEnumAttribute : public ::testing::Test
{
  public:
    const auto& getPossibleValues(const BIOSEnumAttribute& attribute)
    {
        return attribute.possibleValues;
    }

    const auto& getDefaultValue(const BIOSEnumAttribute& attribute)
    {
        return attribute.defaultValue;
    }
};

TEST_F(TestBIOSEnumAttribute, CtorTest)
{
    auto jsonEnumReadOnly = R"({
         "attribute_name" : "CodeUpdatePolicy",
         "possible_values" : [ "Concurrent", "Disruptive" ],
         "default_values" : [ "Concurrent" ]
      })"_json;

    BIOSEnumAttribute enumReadOnly{jsonEnumReadOnly, nullptr};
    EXPECT_EQ(enumReadOnly.name, "CodeUpdatePolicy");
    EXPECT_TRUE(enumReadOnly.readOnly);
    EXPECT_THAT(getPossibleValues(enumReadOnly),
                ElementsAreArray({"Concurrent", "Disruptive"}));
    EXPECT_EQ(getDefaultValue(enumReadOnly), "Concurrent");

    auto jsonEnumReadOnlyError = R"({
         "attribute_name" : "CodeUpdatePolicy",
         "possible_value" : [ "Concurrent", "Disruptive" ],
         "default_values" : [ "Concurrent" ]
      })"_json; // possible_value -> possible_values
    EXPECT_THROW((BIOSEnumAttribute{jsonEnumReadOnlyError, nullptr}),
                 Json::exception);

    auto jsonEnumReadWrite = R"({
         "attribute_name" : "FWBootSide",
         "possible_values" : [ "Perm", "Temp" ],
         "default_values" : [ "Perm" ],
         "dbus":
            {
               "object_path" : "/xyz/abc/def",
               "interface" : "xyz.openbmc.FWBoot.Side",
               "property_name" : "Side",
               "property_type" : "bool",
               "property_values" : [true, false]
            }
      })"_json;

    BIOSEnumAttribute enumReadWrite{jsonEnumReadWrite, nullptr};
    EXPECT_EQ(enumReadWrite.name, "FWBootSide");
    EXPECT_TRUE(!enumReadWrite.readOnly);
}

TEST_F(TestBIOSEnumAttribute, ConstructEntry)
{
    auto jsonEnumReadOnly = R"({
         "attribute_name" : "CodeUpdatePolicy",
         "possible_values" : [ "Concurrent", "Disruptive" ],
         "default_values" : [ "Disruptive" ]
      })"_json;

    std::vector<uint8_t> expectedAttrEntry{
        0,    0, /* attr handle */
        0x80,    /* attr type enum read-only*/
        4,    0, /* attr name handle */
        2,       /* number of possible value */
        2,    0, /* possible value handle */
        3,    0, /* possible value handle */
        1,       /* number of default value */
        1        /* defaut value string handle index */
    };

    std::vector<uint8_t> expectedAttrValueEntry{
        0, 0, /* attr handle */
        0x80, /* attr type */
        1,    /* number of current value */
        1     /* current value string handle index */
    };

    BIOSEnumAttribute enumReadOnly{jsonEnumReadOnly, nullptr};
    Table attrEntry, attrValueEntry;

    MockBIOSStringTable biosStringTable;
    EXPECT_CALL(biosStringTable, findHandle(_))
        .WillOnce(Return(2))
        .WillOnce(Return(3))
        .WillOnce(Return(4));

    enumReadOnly.constructEntry(biosStringTable, attrEntry, attrValueEntry);
    auto attrHeader = BIOSAttrTable::decodeHeader(
        reinterpret_cast<const pldm_bios_attr_table_entry*>(attrEntry.data()));
    auto attrValueHeader = BIOSAttrValTable::decodeHeader(
        reinterpret_cast<const pldm_bios_attr_val_table_entry*>(
            attrValueEntry.data()));

    EXPECT_EQ(attrHeader.attrHandle, attrValueHeader.attrHandle);
    EXPECT_THAT(attrEntry, ElementsAreArray(expectedAttrEntry));
    EXPECT_THAT(attrValueEntry, ElementsAreArray(expectedAttrValueEntry));
}