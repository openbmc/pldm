#include "libpldmresponder/bios.hpp"
#include "libpldmresponder/bios_parser.hpp"
#include "libpldmresponder/bios_table.hpp"

#include <string.h>

#include <array>
#include <cstring>
#include <ctime>
#include <filesystem>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

using namespace pldm;
using namespace pldm::responder;
using namespace pldm::responder::bios;
using namespace pldm::responder::utils;
using namespace bios_parser;

TEST(epochToBCDTime, testTime)
{
    struct tm time
    {
    };
    time.tm_year = 119;
    time.tm_mon = 3;
    time.tm_mday = 13;
    time.tm_hour = 5;
    time.tm_min = 18;
    time.tm_sec = 13;
    time.tm_isdst = -1;

    time_t epochTime = mktime(&time);
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    epochToBCDTime(epochTime, seconds, minutes, hours, day, month, year);

    ASSERT_EQ(0x13, seconds);
    ASSERT_EQ(0x18, minutes);
    ASSERT_EQ(0x5, hours);
    ASSERT_EQ(0x13, day);
    ASSERT_EQ(0x4, month);
    ASSERT_EQ(0x2019, year);
}

TEST(timeToEpoch, testTime0)
{
    std::time_t ret = 1555132693;

    uint8_t sec = 13;
    uint8_t min = 18;
    uint8_t hours = 5;
    uint8_t day = 13;
    uint8_t month = 4;
    uint16_t year = 2019;

    std::time_t timeSec = 0;
    timeSec = timeToEpoch(sec, min, hours, day, month, year);

    EXPECT_EQ(ret, timeSec);
}

TEST(GetBIOSStrings, allScenarios)
{
    using namespace bios_parser;
    // All the BIOS Strings in the BIOS JSON config files.
    Strings vec{"HMCManagedState",
                "On",
                "Off",
                "FWBootSide",
                "Perm",
                "Temp",
                "InbandCodeUpdate",
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

    Strings nullVec{};

    // Invalid directory
    bios_parser::setupConfig("./bios_json_invalid");
    auto strings = bios_parser::getStrings();
    ASSERT_EQ(strings == nullVec, true);

    bios_parser::setupConfig("./bios_jsons");
    strings = bios_parser::getStrings();
    std::sort(strings.begin(), strings.end());
    std::sort(vec.begin(), vec.end());
    ASSERT_EQ(strings == vec, true);
}

TEST(getAttrValue, enumScenarios)
{
    using namespace bios_parser::bios_enum;
    // All the BIOS Strings in the BIOS JSON config files.
    AttrValuesMap valueMap{
        {"HMCManagedState", {false, {"On", "Off"}, {"On"}}},
        {"FWBootSide", {false, {"Perm", "Temp"}, {"Perm"}}},
        {"InbandCodeUpdate", {false, {"Allowed", "NotAllowed"}, {"Allowed"}}},
        {"CodeUpdatePolicy",
         {true, {"Concurrent", "Disruptive"}, {"Concurrent"}}}};

    auto values = bios_parser::bios_enum::getValues();
    ASSERT_EQ(valueMap == values, true);

    bios_parser::bios_enum::CurrentValues cv{"Concurrent"};
    auto value = bios_parser::bios_enum::getAttrValue("CodeUpdatePolicy");
    ASSERT_EQ(value == cv, true);

    // Invalid attribute name
    ASSERT_THROW(bios_parser::bios_enum::getAttrValue("CodeUpdatePolic"),
                 std::out_of_range);
}

TEST(getAttrValue, stringScenarios)
{
    // All the BIOS Strings in the BIOS JSON config files.
    bios_parser::bios_string::AttrValuesMap valueMap{
        {"str_example1", {false, 1, 1, 100, 3, "abc"}},
        {"str_example2", {false, 2, 0, 100, 0, ""}},
        {"str_example3", {true, 0, 1, 100, 2, "ef"}}};

    auto values = bios_parser::bios_string::getValues();
    ASSERT_EQ(valueMap == values, true);

    // Test the attribute without dbus
    std::string cv = "ef";
    auto value = bios_parser::bios_string::getAttrValue("str_example3");
    EXPECT_EQ(value, cv);

    // Invalid attribute name
    ASSERT_THROW(bios_parser::bios_string::getAttrValue("str_example"),
                 std::out_of_range);
}

TEST(getAttrValue, integerScenarios)
{
    using namespace bios_parser::bios_integer;
    AttrValuesMap valueMap{
        {"VDD_AVSBUS_RAIL", {false, 0, 15, 1, 0}},
        {"SBE_IMAGE_MINIMUM_VALID_ECS", {true, 1, 15, 1, 2}}};

    auto values = getValues();
    EXPECT_EQ(valueMap, values);
    auto value = getAttrValue("SBE_IMAGE_MINIMUM_VALID_ECS");
    EXPECT_EQ(value, 2);

    EXPECT_THROW(getAttrValue("VDM"), std::out_of_range);
}

TEST(traverseBIOSTable, attrTableScenarios)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        1, 0, /* attr name handle */
        2,    /* number of possible value */
        2, 0, /* possible value handle */
        3, 0, /* possible value handle */
        1,    /* number of default value */
        0     /* defaut value string handle index */
    };
    std::vector<uint8_t> stringEntry{
        4,   0,       /* attr handle */
        1,            /* attr type */
        12,  0,       /* attr name handle */
        1,            /* string type */
        1,   0,       /* minimum length of the string in bytes */
        100, 0,       /* maximum length of the string in bytes */
        3,   0,       /* length of default string in length */
        'a', 'b', 'c' /* default string  */
    };
    std::vector<uint8_t> table;
    table.insert(table.end(), enumEntry.begin(), enumEntry.end());
    table.insert(table.end(), stringEntry.begin(), stringEntry.end());
    auto padSize = ((table.size() % 4) ? (4 - table.size() % 4) : 0);

    table.insert(table.end(), padSize, 0);
    table.insert(table.end(), sizeof(uint32_t) /*checksum*/, 0);

    pldm::responder::bios::traverseBIOSAttrTable(
        table, [&](const struct pldm_bios_attr_table_entry* entry) {
            int rc;
            switch (entry->attr_type)
            {
                case 0:
                    rc = std::memcmp(entry, enumEntry.data(), enumEntry.size());
                    EXPECT_EQ(rc, 0);
                    break;
                case 1:
                    rc = std::memcmp(entry, stringEntry.data(),
                                     stringEntry.size());
                    EXPECT_EQ(rc, 0);
                    break;
                default:
                    break;
            }
        });
}

namespace fs = std::filesystem;
class TestAllBIOSTables : public ::testing::Test
{
  public:
    static void SetUpTestCase() // will execute once at the begining of all
                                // TestAllBIOSTables objects
    {
        char tmpdir[] = "/tmp/allBiosTables.XXXXXX";
        biosPath = fs::path(mkdtemp(tmpdir));
    }

    static void TearDownTestCase() // will be executed once at th eend of all
                                   // TestAllBIOSTables objects
    {
        fs::remove_all(biosPath);
    }

    static fs::path biosPath;
};

fs::path TestAllBIOSTables::biosPath;

TEST_F(TestAllBIOSTables, GetBIOSTableTestBadRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req* req =
        (struct pldm_get_bios_table_req*)request->payload;
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = 0xFF;

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    auto response = internal::buildBIOSTables(request, requestPayloadLength,
                                              "./bios_jsons", biosPath.c_str());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);

    ASSERT_EQ(PLDM_INVALID_BIOS_TABLE_TYPE, resp->completion_code);
}

TEST_F(TestAllBIOSTables, buildBIOSTablesTestBadRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req* req =
        (struct pldm_get_bios_table_req*)request->payload;
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_ATTR_VAL_TABLE;

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    auto response = internal::buildBIOSTables(request, requestPayloadLength,
                                              "./bios_jsons", biosPath.c_str());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);
    ASSERT_EQ(PLDM_BIOS_TABLE_UNAVAILABLE, resp->completion_code);

    req->table_type = PLDM_BIOS_ATTR_TABLE;
    response = internal::buildBIOSTables(request, requestPayloadLength,
                                         "./bios_jsons", biosPath.c_str());
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    resp = reinterpret_cast<struct pldm_get_bios_table_resp*>(
        responsePtr->payload);
    ASSERT_EQ(PLDM_BIOS_TABLE_UNAVAILABLE, resp->completion_code);
}

TEST_F(TestAllBIOSTables, GetBIOSStringTableTestGoodRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req* req =
        (struct pldm_get_bios_table_req*)request->payload;
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_STRING_TABLE;

    Strings biosStrings = getStrings();
    std::sort(biosStrings.begin(), biosStrings.end());
    biosStrings.erase(std::unique(biosStrings.begin(), biosStrings.end()),
                      biosStrings.end());

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);
    uint8_t times = 0;
    while (times < 2)
    { // first time fresh table second time existing table
        auto response = internal::buildBIOSTables(
            request, requestPayloadLength, "./bios_jsons", biosPath.c_str());
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

        struct pldm_get_bios_table_resp* resp =
            reinterpret_cast<struct pldm_get_bios_table_resp*>(
                responsePtr->payload);

        ASSERT_EQ(0, resp->completion_code);
        ASSERT_EQ(0, resp->next_transfer_handle);
        ASSERT_EQ(PLDM_START_AND_END, resp->transfer_flag);

        uint16_t strLen = 0;
        uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);

        for (auto elem : biosStrings)
        {
            struct pldm_bios_string_table_entry* ptr =
                reinterpret_cast<struct pldm_bios_string_table_entry*>(
                    tableData);
            strLen = ptr->string_length;
            ASSERT_EQ(strLen, elem.size());
            ASSERT_EQ(0, memcmp(elem.c_str(), ptr->name, strLen));
            tableData += (sizeof(pldm_bios_string_table_entry) - 1) + strLen;

        } // end for
        times++;
    }
}

TEST_F(TestAllBIOSTables, getBIOSAttributeTableTestGoodRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req* req =
        (struct pldm_get_bios_table_req*)request->payload;
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_ATTR_TABLE;

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t times = 0;
    while (times < 2)
    { // first time fresh table second time existing table
        auto response = internal::buildBIOSTables(
            request, requestPayloadLength, "./bios_jsons", biosPath.c_str());
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

        struct pldm_get_bios_table_resp* resp =
            reinterpret_cast<struct pldm_get_bios_table_resp*>(
                responsePtr->payload);

        ASSERT_EQ(0, resp->completion_code);
        ASSERT_EQ(0, resp->next_transfer_handle);
        ASSERT_EQ(PLDM_START_AND_END, resp->transfer_flag);

        uint32_t attrTableLen =
            response.size() - sizeof(pldm_msg_hdr) -
            (sizeof(resp->completion_code) +
             sizeof(resp->next_transfer_handle) + sizeof(resp->transfer_flag));
        uint32_t traversed = 0;
        uint16_t attrHdl = 0;
        uint16_t stringHdl = 0;
        uint8_t attrType = 0;
        uint8_t numPosVals = 0;
        uint8_t numDefVals = 0;
        uint8_t defIndex = 0;

        uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
        while (1)
        {
            struct pldm_bios_attr_table_entry* ptr =
                reinterpret_cast<struct pldm_bios_attr_table_entry*>(tableData);
            attrHdl = ptr->attr_handle;
            attrType = ptr->attr_type;
            EXPECT_EQ(0, attrHdl);
            EXPECT_EQ(PLDM_BIOS_ENUMERATION_READ_ONLY, attrType);
            stringHdl = ptr->string_handle;
            EXPECT_EQ(stringHdl, 1);
            tableData += sizeof(attrHdl) + sizeof(attrType) + sizeof(stringHdl);
            numPosVals = *tableData;
            EXPECT_EQ(numPosVals, 2);
            traversed += sizeof(attrHdl) + sizeof(attrType) + sizeof(stringHdl);
            traversed += sizeof(numPosVals);
            PossibleValuesByHandle possiVals;
            tableData++;
            uint16_t* temp = reinterpret_cast<uint16_t*>(tableData);
            uint32_t i = 0;
            while (i < numPosVals)
            {
                uint16_t val = *temp;
                possiVals.push_back(val);
                temp++;
                i++;
            }
            EXPECT_EQ(possiVals.size(), 2);
            tableData += numPosVals * sizeof(stringHdl);
            traversed += numPosVals * sizeof(stringHdl);
            numDefVals = *tableData;
            EXPECT_EQ(numDefVals, 1);
            tableData += numDefVals * sizeof(defIndex);
            traversed += numDefVals + sizeof(numDefVals);

            break; // test for first row only

            if ((attrTableLen - traversed) < 8)
            {
                // reached pad
                break;
            }

        } // end while
        times++;
    }

} // end TEST

TEST_F(TestAllBIOSTables, getBIOSAttributeValueTableTestGoodRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req* req =
        (struct pldm_get_bios_table_req*)request->payload;
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_ATTR_VAL_TABLE;

    std::string attrName("CodeUpdatePolicy");
    bios_enum::CurrentValues currVals = bios_enum::getAttrValue(attrName);

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t times = 0;
    while (times < 2)
    { // first time frest table second time existing table
        auto response = internal::buildBIOSTables(
            request, requestPayloadLength, "./bios_jsons", biosPath.c_str());
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

        struct pldm_get_bios_table_resp* resp =
            reinterpret_cast<struct pldm_get_bios_table_resp*>(
                responsePtr->payload);

        ASSERT_EQ(0, resp->completion_code);
        ASSERT_EQ(0, resp->next_transfer_handle);
        ASSERT_EQ(PLDM_START_AND_END, resp->transfer_flag);

        uint32_t attrValTableLen =
            response.size() - sizeof(pldm_msg_hdr) -
            (sizeof(resp->completion_code) +
             sizeof(resp->next_transfer_handle) + sizeof(resp->transfer_flag));
        uint32_t traversed = 0;
        uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);

        uint16_t attrHdl;
        uint8_t attrType;
        uint8_t numCurrVals;
        uint8_t currValStrIndex;

        while (1)
        {
            struct pldm_bios_attr_val_table_entry* ptr =
                reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(
                    tableData);
            attrHdl = ptr->attr_handle;
            attrType = ptr->attr_type;
            EXPECT_EQ(0, attrHdl);
            EXPECT_EQ(PLDM_BIOS_ENUMERATION_READ_ONLY, attrType);
            tableData += sizeof(attrHdl) + sizeof(attrType);
            traversed += sizeof(attrHdl) + sizeof(attrType);
            numCurrVals = *tableData;
            EXPECT_EQ(1, numCurrVals);
            tableData += sizeof(numCurrVals);
            traversed += sizeof(numCurrVals);
            currValStrIndex = *tableData;
            EXPECT_EQ(0, currValStrIndex);
            tableData += numCurrVals;
            traversed += numCurrVals;
            break; // testing for first row
            if ((attrValTableLen - traversed) < 8)
            {
                break;
            }
        }
        times++;
    }

} // end TEST

class TestSingleTypeBIOSTable : public ::testing::Test
{
  public:
    void SetUp() override
    { // will be executed before each individual test defined
        // in TestSingleTypeBIOSTable
        char tmpdir[] = "/tmp/singleTypeBIOSTable.XXXXXX";
        destBiosPath = fs::path(mkdtemp(tmpdir));
    }

    void TearDown() override
    { // will be executed after each individual test
        // defined in TestSingleTypeBIOSTable
        fs::remove_all(destBiosPath);
    }

    void CopySingleJsonFile(std::string file)
    {
        fs::path srcDir("./bios_jsons");
        fs::path srcBiosPath = srcDir / file;
        std::filesystem::copy(srcBiosPath, destBiosPath);
    }

    fs::path destBiosPath;
};

TEST_F(TestSingleTypeBIOSTable, getBIOSAttributeValueTableBasedOnStringTypeTest)
{
    // Copy string json file to the destination
    TestSingleTypeBIOSTable::CopySingleJsonFile(bios_parser::bIOSStrJson);
    auto fpath = TestSingleTypeBIOSTable::destBiosPath.c_str();

    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req* req =
        (struct pldm_get_bios_table_req*)request->payload;

    // Get string table with string json file only
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_STRING_TABLE;

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);
    auto str_response =
        internal::buildBIOSTables(request, requestPayloadLength, fpath, fpath);

    // Get attribute table with string json file only
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_ATTR_TABLE;

    auto attr_response =
        internal::buildBIOSTables(request, requestPayloadLength, fpath, fpath);

    // Get attribute value table with string type
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_ATTR_VAL_TABLE;

    // Test attribute str_example3 here, which has no dbus
    for (uint8_t times = 0; times < 2; times++)
    { // first time first table second time existing table
        auto response = internal::buildBIOSTables(request, requestPayloadLength,
                                                  fpath, fpath);
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

        struct pldm_get_bios_table_resp* resp =
            reinterpret_cast<struct pldm_get_bios_table_resp*>(
                responsePtr->payload);

        ASSERT_EQ(0, resp->completion_code);
        ASSERT_EQ(0, resp->next_transfer_handle);
        ASSERT_EQ(PLDM_START_AND_END, resp->transfer_flag);

        uint32_t attrValTableLen =
            response.size() - sizeof(pldm_msg_hdr) -
            (sizeof(struct pldm_get_bios_table_resp) - 1);
        uint32_t traversed = 0;
        uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);

        while (true)
        {
            struct pldm_bios_attr_val_table_entry* ptr =
                reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(
                    tableData);
            uint16_t attrHdl = ptr->attr_handle;
            uint8_t attrType = ptr->attr_type;
            EXPECT_EQ(PLDM_BIOS_STRING_READ_ONLY, attrType);
            tableData += sizeof(attrHdl) + sizeof(attrType);
            traversed += sizeof(attrHdl) + sizeof(attrType);
            auto sizeDefaultStr = *(reinterpret_cast<uint16_t*>(tableData));
            EXPECT_EQ(2, sizeDefaultStr);
            tableData += sizeof(uint16_t);
            traversed += sizeof(uint16_t);
            EXPECT_EQ('e', *tableData);
            EXPECT_EQ('f', *(tableData + 1));
            tableData += sizeDefaultStr;
            traversed += sizeDefaultStr;
            break; // testing for first row
            if ((attrValTableLen - traversed) < 8)
            {
                break;
            }
        }
    }

} // end TEST

TEST_F(TestSingleTypeBIOSTable,
       getBIOSAttributeValueTableBasedOnIntegerTypeTest)
{
    // Copy integer json file to the destination
    TestSingleTypeBIOSTable::CopySingleJsonFile(bios_parser::bIOSIntegerJson);
    auto fpath = TestSingleTypeBIOSTable::destBiosPath.c_str();

    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req* req =
        (struct pldm_get_bios_table_req*)request->payload;

    // Get string table with integer json file only
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_STRING_TABLE;

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);
    internal::buildBIOSTables(request, requestPayloadLength, fpath, fpath);

    // Get attribute table with integer json file only
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_ATTR_TABLE;

    auto attr_response =
        internal::buildBIOSTables(request, requestPayloadLength, fpath, fpath);

    // Get attribute value table with integer type
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_ATTR_VAL_TABLE;

    // Test attribute SBE_IMAGE_MINIMUM_VALID_ECS here, which has no dbus
    for (uint8_t times = 0; times < 2; times++)
    { // first time first table second time existing table
        auto response = internal::buildBIOSTables(request, requestPayloadLength,
                                                  fpath, fpath);
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

        struct pldm_get_bios_table_resp* resp =
            reinterpret_cast<struct pldm_get_bios_table_resp*>(
                responsePtr->payload);

        EXPECT_EQ(0, resp->completion_code);
        EXPECT_EQ(0, resp->next_transfer_handle);
        EXPECT_EQ(PLDM_START_AND_END, resp->transfer_flag);
        uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
        auto ptr =
            reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(tableData);
        uint16_t attrHdl = ptr->attr_handle;
        uint8_t attrType = ptr->attr_type;
        EXPECT_EQ(PLDM_BIOS_INTEGER_READ_ONLY, attrType);
        tableData += sizeof(attrHdl) + sizeof(attrType);
        auto cv = *(reinterpret_cast<uint64_t*>(tableData));
        EXPECT_EQ(2, cv);
    }
}
