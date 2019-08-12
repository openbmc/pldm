#include "libpldmresponder/bios.hpp"
#include "libpldmresponder/bios_parser.hpp"
#include "libpldmresponder/bios_table.hpp"
#include "registration.hpp"

#include <string.h>

#include <array>
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
using namespace bios_parser::bios_enum;

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

TEST(GetBIOSStrings, allScenarios)
{
    using namespace bios_parser;
    // All the BIOS Strings in the BIOS JSON config files.
    Strings vec{"HMCManagedState",  "On",         "Off",
                "FWBootSide",       "Perm",       "Temp",
                "InbandCodeUpdate", "Allowed",    "NotAllowed",
                "CodeUpdatePolicy", "Concurrent", "Disruptive"};

    Strings nullVec{};

    // Invalid directory
    auto strings = bios_parser::getStrings("./bios_json");
    ASSERT_EQ(strings == nullVec, true);

    strings = bios_parser::getStrings("./bios_jsons");
    ASSERT_EQ(strings == vec, true);
}

TEST(getAttrValue, allScenarios)
{
    using namespace bios_parser::bios_enum;
    // All the BIOS Strings in the BIOS JSON config files.
    AttrValuesMap valueMap{
        {"HMCManagedState", {false, {"On", "Off"}, {"On"}}},
        {"FWBootSide", {false, {"Perm", "Temp"}, {"Perm"}}},
        {"InbandCodeUpdate", {false, {"Allowed", "NotAllowed"}, {"Allowed"}}},
        {"CodeUpdatePolicy",
         {false, {"Concurrent", "Disruptive"}, {"Concurrent"}}}};

    auto rc = setupValueLookup("./bios_jsons");
    ASSERT_EQ(rc, 0);

    auto values = getValues();
    ASSERT_EQ(valueMap == values, true);

    CurrentValues cv{"Concurrent"};
    auto value = getAttrValue("CodeUpdatePolicy");
    ASSERT_EQ(value == cv, true);

    // Invalid attribute name
    ASSERT_THROW(getAttrValue("CodeUpdatePolic"), std::out_of_range);
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

    Interfaces intfs{};
    Request r{0, request};
    auto response = internal::buildBIOSTables(intfs, r, requestPayloadLength,
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

    Interfaces intfs{};
    Request r{0, request};
    auto response = internal::buildBIOSTables(intfs, r, requestPayloadLength,
                                              "./bios_jsons", biosPath.c_str());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);
    ASSERT_EQ(PLDM_BIOS_TABLE_UNAVAILABLE, resp->completion_code);

    req->table_type = PLDM_BIOS_ATTR_TABLE;
    response = internal::buildBIOSTables(intfs, r, requestPayloadLength,
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

    Strings biosStrings = getStrings("./bios_jsons");
    std::sort(biosStrings.begin(), biosStrings.end());
    biosStrings.erase(std::unique(biosStrings.begin(), biosStrings.end()),
                      biosStrings.end());

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);
    uint8_t times = 0;
    while (times < 2)
    { // first time fresh table second time existing table
        Interfaces intfs{};
        Request req{0, request};
        auto response = internal::buildBIOSTables(
            intfs, req, requestPayloadLength, "./bios_jsons", biosPath.c_str());
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
        Interfaces intfs{};
        Request req{0, request};
        auto response = internal::buildBIOSTables(
            intfs, req, requestPayloadLength, "./bios_jsons", biosPath.c_str());
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
            EXPECT_EQ(PLDM_BIOS_ENUMERATION, attrType);
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
    CurrentValues currVals = getAttrValue(attrName);

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t times = 0;
    while (times < 2)
    { // first time frest table second time existing table
        Interfaces intfs{};
        Request req{0, request};
        auto response = internal::buildBIOSTables(
            intfs, req, requestPayloadLength, "./bios_jsons", biosPath.c_str());
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
            EXPECT_EQ(PLDM_BIOS_ENUMERATION, attrType);
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
