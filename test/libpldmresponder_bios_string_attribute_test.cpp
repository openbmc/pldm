#include "libpldmresponder/bios_string_attribute.hpp"

#include <memory>
#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::responder::bios;

class TestBIOSStringAttribute : public ::testing::Test
{
  public:
    const std::string&
        getDefaultString(const BIOSStringAttribute& biosStringAttribute)
    {
        return biosStringAttribute.defaultString;
    }

    uint8_t getStringType(const BIOSStringAttribute& biosStringAttribute)
    {
        return biosStringAttribute.stringType;
    }
    uint16_t getMinLength(const BIOSStringAttribute& biosStringAttribute)
    {
        return biosStringAttribute.minStringLength;
    }
    uint16_t getMaxLength(const BIOSStringAttribute& biosStringAttribute)
    {
        return biosStringAttribute.maxStringLength;
    }
    uint16_t getDefLength(const BIOSStringAttribute& biosStringAttribute)
    {
        return biosStringAttribute.defaultStringLength;
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

    BIOSStringAttribute stringReadOnly{jsonStringReadOnly};
    EXPECT_EQ(stringReadOnly.name, "str_example3");
    EXPECT_TRUE(stringReadOnly.readOnly);
    EXPECT_EQ(getStringType(stringReadOnly), BIOSStringAttribute::ASCII);
    EXPECT_EQ(getMinLength(stringReadOnly), 1);
    EXPECT_EQ(getMaxLength(stringReadOnly), 100);
    EXPECT_EQ(getDefLength(stringReadOnly), 2);
    EXPECT_EQ(getDefaultString(stringReadOnly), "ef");

    auto jsonStringReadOnlyError = R"(  {
            "attribute_name" : "str_example3",
            "string_type" : "ASCII",
            "minimum_string_length" : 1,
            "maximum_string_length" : 100,
            "default_string" : "ef"
        })"_json; // missing default_string_length
    EXPECT_THROW(BIOSStringAttribute{jsonStringReadOnlyError}, Json::exception);

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
    BIOSStringAttribute stringReadWrite{jsonStringReadWrite};

    EXPECT_EQ(stringReadWrite.name, "str_example1");
    EXPECT_TRUE(!stringReadWrite.readOnly);
}

class MockBIOSStringTable : public BIOSStringTable
{
  public:
    MockBIOSStringTable() : BIOSStringTable({})
    {
    }
    MOCK_CONST_METHOD1(findString, std::string(uint16_t));
    MOCK_CONST_METHOD1(findHandle, uint16_t(const std::string&));
    MOCK_CONST_METHOD1(decodeHandle,
                       uint16_t(const pldm_bios_string_table_entry*));
    MOCK_CONST_METHOD1(decodeString,
                       std::string(const pldm_bios_string_table_entry*));
};

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
    BIOSStringAttribute stringReadWrite{jsonStringReadWrite};

    MockBIOSStringTable biosStringTable;

    ON_CALL(biosStringTable, findHandle(_)).WillByDefault(Return(5));
    EXPECT_CALL(biosStringTable, findHandle(_)).WillOnce(Return(5));

    Table attrEntry, attrValueEntry;

    stringReadWrite.constructEntry(biosStringTable, attrEntry, attrValueEntry);

    for (auto v : attrEntry)
    {
        std::cerr << (int)v << std::endl;
    }
}
