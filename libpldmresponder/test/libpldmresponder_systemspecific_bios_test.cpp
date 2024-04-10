#include "common/bios_utils.hpp"
#include "common/test/mocked_utils.hpp"
#include "libpldmresponder/bios_config.hpp"
#include "libpldmresponder/bios_string_attribute.hpp"
#include "libpldmresponder/oem_handler.hpp"
#include "libpldmresponder/platform_config.hpp"
#include "mocked_bios.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::bios::utils;
using namespace pldm::responder::bios;
using namespace pldm::utils;

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Throw;

class TestSystemSpecificBIOSConfig : public ::testing::Test
{
  public:
    static void SetUpTestCase() // will execute once at the begining of all
                                // TestSystemSpecificBIOSConfig objects
    {
        char tmpdir[] = "/tmp/BIOSTables.XXXXXX";
        tableDir = fs::path(mkdtemp(tmpdir));

        std::vector<fs::path> paths = {
            "./bios_jsons/string_attrs.json",
            "./bios_jsons/integer_attrs.json",
            "./bios_jsons/enum_attrs.json",
        };

        for (auto& path : paths)
        {
            std::ifstream file;
            file.open(path);
            auto j = Json::parse(file);
            jsons.emplace_back(j);
        }
    }

    std::optional<Json> findJsonEntry(const std::string& name)
    {
        for (auto& json : jsons)
        {
            auto entries = json.at("entries");
            for (auto& entry : entries)
            {
                auto n = entry.at("attribute_name").get<std::string>();
                if (n == name)
                {
                    return entry;
                }
            }
        }
        return std::nullopt;
    }

    static void TearDownTestCase() // will be executed once at th end of all
                                   // TestSystemSpecificBIOSConfig objects
    {
        fs::remove_all(tableDir);
    }

    static fs::path tableDir;
    static std::vector<Json> jsons;
};

fs::path TestSystemSpecificBIOSConfig::tableDir;
std::vector<Json> TestSystemSpecificBIOSConfig::jsons;

class MockSystemConfig : public pldm::responder::platform_config::Handler
{
  public:
    MockSystemConfig() {}
    MOCK_METHOD(void, ibmCompatibleAddedCallback, (sdbusplus::message_t&), ());
    MOCK_METHOD(std::optional<std::filesystem::path>, getPlatformName, ());
};

TEST_F(TestSystemSpecificBIOSConfig, buildTablesTest)
{
    MockdBusHandler dbusHandler;
    MockSystemConfig mockSystemConfig;
    std::string biosFilePath("./");

    EXPECT_CALL(mockSystemConfig, getPlatformName())
        .WillOnce(Return(std::filesystem::path("bios_jsons")));

    BIOSConfig biosConfig(biosFilePath.c_str(), tableDir.c_str(), &dbusHandler,
                          0, 0, nullptr, nullptr, &mockSystemConfig, []() {});
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

    BIOSStringTable biosStringTable(*stringTable);

    for (auto entry : BIOSTableIter<PLDM_BIOS_ATTR_TABLE>(attrTable->data(),
                                                          attrTable->size()))
    {
        auto header = table::attribute::decodeHeader(entry);
        auto attrName = biosStringTable.findString(header.stringHandle);
        auto jsonEntry = findJsonEntry(attrName);
        EXPECT_TRUE(jsonEntry);
        switch (header.attrType)
        {
            case PLDM_BIOS_STRING:
            case PLDM_BIOS_STRING_READ_ONLY:
            {
                auto stringField = table::attribute::decodeStringEntry(entry);
                auto stringType = BIOSStringAttribute::strTypeMap.at(
                    jsonEntry->at("string_type").get<std::string>());
                EXPECT_EQ(stringField.stringType,
                          static_cast<uint8_t>(stringType));

                EXPECT_EQ(
                    stringField.minLength,
                    jsonEntry->at("minimum_string_length").get<uint16_t>());
                EXPECT_EQ(
                    stringField.maxLength,
                    jsonEntry->at("maximum_string_length").get<uint16_t>());
                EXPECT_EQ(
                    stringField.defLength,
                    jsonEntry->at("default_string_length").get<uint16_t>());
                EXPECT_EQ(stringField.defString,
                          jsonEntry->at("default_string").get<std::string>());
                break;
            }
            case PLDM_BIOS_INTEGER:
            case PLDM_BIOS_INTEGER_READ_ONLY:
            {
                auto integerField = table::attribute::decodeIntegerEntry(entry);
                EXPECT_EQ(integerField.lowerBound,
                          jsonEntry->at("lower_bound").get<uint64_t>());
                EXPECT_EQ(integerField.upperBound,
                          jsonEntry->at("upper_bound").get<uint64_t>());
                EXPECT_EQ(integerField.scalarIncrement,
                          jsonEntry->at("scalar_increment").get<uint32_t>());
                EXPECT_EQ(integerField.defaultValue,
                          jsonEntry->at("default_value").get<uint64_t>());
                break;
            }
            case PLDM_BIOS_ENUMERATION:
            case PLDM_BIOS_ENUMERATION_READ_ONLY:
            {
                auto [pvHdls,
                      defInds] = table::attribute::decodeEnumEntry(entry);
                auto possibleValues = jsonEntry->at("possible_values")
                                          .get<std::vector<std::string>>();
                std::vector<std::string> strings;
                for (auto pv : pvHdls)
                {
                    auto s = biosStringTable.findString(pv);
                    strings.emplace_back(s);
                }
                EXPECT_EQ(strings, possibleValues);
                EXPECT_EQ(defInds.size(), 1);

                auto defValue = biosStringTable.findString(pvHdls[defInds[0]]);
                auto defaultValues = jsonEntry->at("default_values")
                                         .get<std::vector<std::string>>();
                EXPECT_EQ(defValue, defaultValues[0]);

                break;
            }
            default:
                EXPECT_TRUE(false);
                break;
        }
    }

    for (auto entry : BIOSTableIter<PLDM_BIOS_ATTR_VAL_TABLE>(
             attrValueTable->data(), attrValueTable->size()))
    {
        auto header = table::attribute_value::decodeHeader(entry);
        auto attrEntry = table::attribute::findByHandle(*attrTable,
                                                        header.attrHandle);
        auto attrHeader = table::attribute::decodeHeader(attrEntry);
        auto attrName = biosStringTable.findString(attrHeader.stringHandle);
        auto jsonEntry = findJsonEntry(attrName);
        EXPECT_TRUE(jsonEntry);
        switch (header.attrType)
        {
            case PLDM_BIOS_STRING:
            case PLDM_BIOS_STRING_READ_ONLY:
            {
                auto value = table::attribute_value::decodeStringEntry(entry);
                auto defValue =
                    jsonEntry->at("default_string").get<std::string>();
                EXPECT_EQ(value, defValue);
                break;
            }
            case PLDM_BIOS_INTEGER:
            case PLDM_BIOS_INTEGER_READ_ONLY:
            {
                auto value = table::attribute_value::decodeIntegerEntry(entry);
                auto defValue = jsonEntry->at("default_value").get<uint64_t>();
                EXPECT_EQ(value, defValue);
                break;
            }
            case PLDM_BIOS_ENUMERATION:
            case PLDM_BIOS_ENUMERATION_READ_ONLY:
            {
                auto indices = table::attribute_value::decodeEnumEntry(entry);
                EXPECT_EQ(indices.size(), 1);
                auto possibleValues = jsonEntry->at("possible_values")
                                          .get<std::vector<std::string>>();

                auto defValues = jsonEntry->at("default_values")
                                     .get<std::vector<std::string>>();
                EXPECT_EQ(possibleValues[indices[0]], defValues[0]);
                break;
            }
            default:
                EXPECT_TRUE(false);
                break;
        }
    }
}

TEST_F(TestSystemSpecificBIOSConfig, buildTablesSystemSpecificTest)
{
    MockdBusHandler dbusHandler;
    MockSystemConfig mockSystemConfig;

    EXPECT_CALL(mockSystemConfig, getPlatformName()).WillOnce(Return(""));
    ON_CALL(dbusHandler, getDbusPropertyVariant(_, _, _))
        .WillByDefault(Throw(std::exception()));

    BIOSConfig biosConfig("./system_type1/bios_jsons", tableDir.c_str(),
                          &dbusHandler, 0, 0, nullptr, nullptr,
                          &mockSystemConfig, []() {});

    auto stringTable = biosConfig.getBIOSTable(PLDM_BIOS_STRING_TABLE);
    auto attrTable = biosConfig.getBIOSTable(PLDM_BIOS_ATTR_TABLE);
    auto attrValueTable = biosConfig.getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);

    EXPECT_TRUE(stringTable);
    EXPECT_TRUE(attrTable);
    EXPECT_TRUE(attrValueTable);

    BIOSStringTable biosStringTable(*stringTable);

    for (auto entry : BIOSTableIter<PLDM_BIOS_ATTR_TABLE>(attrTable->data(),
                                                          attrTable->size()))
    {
        auto header = table::attribute::decodeHeader(entry);
        auto attrName = biosStringTable.findString(header.stringHandle);
        auto jsonEntry = findJsonEntry(attrName);
        EXPECT_TRUE(jsonEntry);
        switch (header.attrType)
        {
            case PLDM_BIOS_STRING:
            case PLDM_BIOS_STRING_READ_ONLY:
            {
                if (attrName == "str_example2")
                {
                    auto stringField =
                        table::attribute::decodeStringEntry(entry);
                    EXPECT_EQ(stringField.maxLength, 200);
                }

                break;
            }
            case PLDM_BIOS_INTEGER:
            case PLDM_BIOS_INTEGER_READ_ONLY:
            {
                if (attrName == "SBE_IMAGE_MINIMUM_VALID_ECS")
                {
                    auto integerField =
                        table::attribute::decodeIntegerEntry(entry);
                    EXPECT_EQ(integerField.upperBound, 30);
                }
                break;
            }
            case PLDM_BIOS_ENUMERATION:
            case PLDM_BIOS_ENUMERATION_READ_ONLY:
            {
                if (attrName == "FWBootSide")
                {
                    auto [pvHdls,
                          defInds] = table::attribute::decodeEnumEntry(entry);
                    auto defValue =
                        biosStringTable.findString(pvHdls[defInds[0]]);
                    EXPECT_EQ(defValue, "Temp");
                }
            }
        }
    }
}

TEST_F(TestSystemSpecificBIOSConfig, setBIOSTable)
{
    MockdBusHandler dbusHandler;
    MockSystemConfig mockSystemConfig;

    EXPECT_CALL(mockSystemConfig, getPlatformName()).WillOnce(Return("jsons"));
    BIOSConfig biosConfig("./", tableDir.c_str(), &dbusHandler, 0, 0, nullptr,
                          nullptr, &mockSystemConfig, []() {});

    std::set<std::string> strings{"pvm_system_name", "pvm_stop_at_standby",
                                  "fw_boot_side", "fw_next_boot_side"};

    Table table;
    for (const auto& elem : strings)
    {
        table::string::constructEntry(table, elem);
    }

    table::appendPadAndChecksum(table);
    auto rc = biosConfig.setBIOSTable(PLDM_BIOS_STRING_TABLE, table);
    EXPECT_EQ(rc, PLDM_SUCCESS);

    auto stringTable = biosConfig.getBIOSTable(PLDM_BIOS_STRING_TABLE);
    EXPECT_TRUE(stringTable);
}

TEST_F(TestSystemSpecificBIOSConfig, getBIOSTableFailure)
{
    MockdBusHandler dbusHandler;
    MockSystemConfig mockSystemConfig;

    EXPECT_CALL(mockSystemConfig, getPlatformName()).WillOnce(Return("jsons"));
    BIOSConfig biosConfig("./", tableDir.c_str(), &dbusHandler, 0, 0, nullptr,
                          nullptr, &mockSystemConfig, []() {});

    auto stringTable = biosConfig.getBIOSTable(PLDM_BIOS_STRING_TABLE);
    auto attrTable = biosConfig.getBIOSTable(PLDM_BIOS_ATTR_TABLE);
    auto attrValueTable = biosConfig.getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);

    EXPECT_FALSE(stringTable);
    EXPECT_FALSE(attrTable);
    EXPECT_FALSE(attrValueTable);
}

TEST_F(TestSystemSpecificBIOSConfig, setAttrValueFailure)
{
    MockdBusHandler dbusHandler;
    MockSystemConfig mockSystemConfig;

    EXPECT_CALL(mockSystemConfig, getPlatformName()).WillOnce(Return("jsons"));
    BIOSConfig biosConfig("./", tableDir.c_str(), &dbusHandler, 0, 0, nullptr,
                          nullptr, &mockSystemConfig, []() {});

    std::vector<uint8_t> attrValueEntry{
        0,   0,             /* attr handle */
        1,                  /* attr type string read-write */
        4,   0,             /* current string length */
        'a', 'b', 'c', 'd', /* defaut value string handle index */
    };

    uint16_t attrHandle{10};
    attrValueEntry[0] = attrHandle & 0xff;
    attrValueEntry[1] = (attrHandle >> 8) & 0xff;

    auto rc = biosConfig.setAttrValue(attrValueEntry.data(),
                                      attrValueEntry.size(), false);
    std::cout << "Error in settig Attribute " << rc << std::endl;
    EXPECT_EQ(rc, PLDM_BIOS_TABLE_UNAVAILABLE);
}

TEST_F(TestSystemSpecificBIOSConfig, setAttrValue)
{
    MockdBusHandler dbusHandler;
    MockSystemConfig mockSystemConfig;

    EXPECT_CALL(mockSystemConfig, getPlatformName())
        .WillOnce(Return(std::filesystem::path("")));

    BIOSConfig biosConfig("./bios_jsons", tableDir.c_str(), &dbusHandler, 0, 0,
                          nullptr, nullptr, &mockSystemConfig, []() {});

    auto stringTable = biosConfig.getBIOSTable(PLDM_BIOS_STRING_TABLE);
    auto attrTable = biosConfig.getBIOSTable(PLDM_BIOS_ATTR_TABLE);

    BIOSStringTable biosStringTable(*stringTable);
    BIOSTableIter<PLDM_BIOS_ATTR_TABLE> attrTableIter(attrTable->data(),
                                                      attrTable->size());
    auto stringHandle = biosStringTable.findHandle("str_example1");
    uint16_t attrHandle{};

    for (auto entry : BIOSTableIter<PLDM_BIOS_ATTR_TABLE>(attrTable->data(),
                                                          attrTable->size()))
    {
        auto header = table::attribute::decodeHeader(entry);
        if (header.stringHandle == stringHandle)
        {
            attrHandle = header.attrHandle;
            break;
        }
    }

    EXPECT_NE(attrHandle, 0);

    std::vector<uint8_t> attrValueEntry{
        0,   0,             /* attr handle */
        1,                  /* attr type string read-write */
        4,   0,             /* current string length */
        'a', 'b', 'c', 'd', /* defaut value string handle index */
    };

    attrValueEntry[0] = attrHandle & 0xff;
    attrValueEntry[1] = (attrHandle >> 8) & 0xff;

    DBusMapping dbusMapping{"/xyz/abc/def",
                            "xyz.openbmc_project.str_example1.value",
                            "Str_example1", "string"};
    PropertyValue value = std::string("abcd");
    EXPECT_CALL(dbusHandler, setDbusProperty(dbusMapping, value)).Times(1);

    auto rc = biosConfig.setAttrValue(attrValueEntry.data(),
                                      attrValueEntry.size(), false);
    EXPECT_EQ(rc, PLDM_SUCCESS);

    auto attrValueTable = biosConfig.getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);
    auto findEntry =
        [&attrValueTable](
            uint16_t handle) -> const pldm_bios_attr_val_table_entry* {
        for (auto entry : BIOSTableIter<PLDM_BIOS_ATTR_VAL_TABLE>(
                 attrValueTable->data(), attrValueTable->size()))
        {
            auto [attrHandle, _] = table::attribute_value::decodeHeader(entry);
            if (attrHandle == handle)
                return entry;
        }
        return nullptr;
    };

    auto entry = findEntry(attrHandle);
    EXPECT_NE(entry, nullptr);

    auto p = reinterpret_cast<const uint8_t*>(entry);
    EXPECT_THAT(std::vector<uint8_t>(p, p + attrValueEntry.size()),
                ElementsAreArray(attrValueEntry));
}
