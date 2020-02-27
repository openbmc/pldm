#include "libpldmresponder/bios_string_attribute.hpp"
#include "mock_utils.hpp"

#include <memory>
#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::responder::bios;
using testing::ElementsAreArray;

class TestBIOSStringAttribute : public ::testing::Test
{
  public:
    const auto& getStringInfo(const BIOSStringAttribute& biosStringAttribute)
    {
        return biosStringAttribute.stringInfo;
    }
};

TEST_F(TestBIOSStringAttribute, CtorTest)
{
    auto jsonStringReadOnly = R"(  {
            "attribute_name" : "str_example3",
            "string_type" : "ASCII",
            "minimum_string_length" : 1,
            "maximum_string_length" : 100,
            "default_string_length" : 2,
            "default_string" : "ef"
        })"_json;
    BIOSStringAttribute stringReadOnly{jsonStringReadOnly, nullptr};
    EXPECT_EQ(stringReadOnly.name, "str_example3");
    EXPECT_TRUE(stringReadOnly.readOnly);

    auto& stringInfo = getStringInfo(stringReadOnly);
    EXPECT_EQ(stringInfo.stringType, stringReadOnly.ASCII);
    EXPECT_EQ(stringInfo.minLength, 1);
    EXPECT_EQ(stringInfo.maxLength, 100);
    EXPECT_EQ(stringInfo.defLength, 2);
    EXPECT_EQ(stringInfo.defString, "ef");

    auto jsonStringReadOnlyError = R"(  {
            "attribute_name" : "str_example3",
            "string_type" : "ASCII",
            "minimum_string_length" : 1,
            "maximum_string_length" : 100,
            "default_string" : "ef"
        })"_json; // missing default_string_length

    EXPECT_THROW((BIOSStringAttribute{jsonStringReadOnlyError, nullptr}),
                 Json::exception);

    auto jsonStringReadWrite = R"({
            "attribute_name" : "str_example1",
            "string_type" : "ASCII",
            "minimum_string_length" : 1,
            "maximum_string_length" : 100,
            "default_string_length" : 3,
            "default_string" : "abc",
            "dbus" : {
                "object_path" : "/xyz/abc/def",
                "interface" : "xyz.openbmc_project.str_example1.value",
                "property_name" : "Str_example1",
                "property_type" : "string"
            }
        })"_json;
    BIOSStringAttribute stringReadWrite{jsonStringReadWrite, nullptr};

    EXPECT_EQ(stringReadWrite.name, "str_example1");
    EXPECT_TRUE(!stringReadWrite.readOnly);
}

using ::testing::_;
using ::testing::Return;

TEST_F(TestBIOSStringAttribute, ConstructEntry)
{
    auto jsonStringReadWrite = R"({
            "attribute_name" : "str_example1",
            "string_type" : "ASCII",
            "minimum_string_length" : 1,
            "maximum_string_length" : 100,
            "default_string_length" : 3,
            "default_string" : "abc",
            "dbus" : {
                "object_path" : "/xyz/abc/def",
                "interface" : "xyz.openbmc_project.str_example1.value",
                "property_name" : "Str_example1",
                "property_type" : "string"
            }
        })"_json;
    BIOSStringAttribute stringReadWrite{jsonStringReadWrite, nullptr};

    MockBIOSStringTable biosStringTable;

    ON_CALL(biosStringTable, findHandle(_)).WillByDefault(Return(5));

    Table attrEntry, attrValueEntry;

    stringReadWrite.constructEntry(biosStringTable, attrEntry, attrValueEntry);

    std::vector<uint8_t> expectedAttrEntry{
        0,   0,       /* attr handle */
        1,            /* attr type */
        5,   0,       /* attr name handle */
        1,            /* string type */
        1,   0,       /* minimum length of the string in bytes */
        100, 0,       /* maximum length of the string in bytes */
        3,   0,       /* length of default string in length */
        'a', 'b', 'c' /* default string  */
    };

    std::vector<uint8_t> expectedAttrValueEntry{
        0,   0,        /* attr handle */
        1,             /* attr type */
        3,   0,        /* current string length */
        'a', 'b', 'c', /* defaut value string handle index */
    };

    auto attrHeader = BIOSAttrTable::decodeHeader(
        reinterpret_cast<const pldm_bios_attr_table_entry*>(attrEntry.data()));
    auto attrValueHeader = BIOSAttrValTable::decodeHeader(
        reinterpret_cast<const pldm_bios_attr_val_table_entry*>(
            attrValueEntry.data()));

    EXPECT_EQ(attrHeader.attrHandle, attrValueHeader.attrHandle);

    /** attr handle is computed by libpldm, set it to be 0 to test */
    attrEntry[0] = 0;
    attrEntry[1] = 0;
    attrValueEntry[0] = 0;
    attrValueEntry[1] = 0;

    EXPECT_THAT(attrEntry, ElementsAreArray(expectedAttrEntry));
    EXPECT_THAT(attrValueEntry, ElementsAreArray(expectedAttrValueEntry));

    auto jsonStringReadOnly = R"({
            "attribute_name" : "str_example1",
            "string_type" : "ASCII",
            "minimum_string_length" : 1,
            "maximum_string_length" : 100,
            "default_string_length" : 3,
            "default_string" : "abc"
        })"_json;

    BIOSStringAttribute stringReadOnly{jsonStringReadOnly, nullptr};

    /** constructEntry constructs an entry at the end of a vector, so set
     * vector'size to be 0 */
    attrEntry.resize(0);
    attrValueEntry.resize(0);

    stringReadOnly.constructEntry(biosStringTable, attrEntry, attrValueEntry);

    attrHeader = BIOSAttrTable::decodeHeader(
        reinterpret_cast<const pldm_bios_attr_table_entry*>(attrEntry.data()));
    attrValueHeader = BIOSAttrValTable::decodeHeader(
        reinterpret_cast<const pldm_bios_attr_val_table_entry*>(
            attrValueEntry.data()));

    /** attr handle is computed by libpldm, set it to be 0 to test */
    attrEntry[0] = 0;
    attrEntry[1] = 0;
    attrValueEntry[0] = 0;
    attrValueEntry[1] = 1;

    /* Set expected attr type to be readonly */
    expectedAttrEntry[2] = PLDM_BIOS_STRING_READ_ONLY;
    expectedAttrValueEntry[2] = PLDM_BIOS_STRING_READ_ONLY;

    EXPECT_EQ(attrHeader.attrHandle, attrValueHeader.attrHandle);
}
