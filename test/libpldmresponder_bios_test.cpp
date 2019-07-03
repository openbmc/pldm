
#include "libpldmresponder/bios.hpp"
#include "libpldmresponder/bios_table.hpp"

#include <string.h>

#include <array>
#include <ctime>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

using namespace pldm::responder;
using namespace pldm::responder::utils;

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

TEST(GetBIOSStringTable, testGoodRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES> requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req *req = (struct pldm_get_bios_table_req *)request->payload;
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_STRING_TABLE;

    Strings biosStrings = getBIOSStrings(BIOS_STRING_TABLE_FILE_PATH);

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    auto response = getBIOSTable(request,requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    struct pldm_get_bios_table_resp* resp = reinterpret_cast<struct pldm_get_bios_table_resp*>(responsePtr->payload);

    ASSERT_EQ(0,resp->completion_code);
    ASSERT_EQ(0,resp->next_transfer_handle);
    ASSERT_EQ(PLDM_START_AND_END,resp->transfer_flag); 

    uint16_t strLen = 0;
    uint8_t *tableData = reinterpret_cast<uint8_t*>(resp->table_data);
    //uint32_t tableLen = response.size() - sizeof(pldm_msg_hdr) - (sizeof(resp->completion_code)+ sizeof(resp->next_transfer_handle) + sizeof(transfer_flag));
    
    for (auto elem : biosStrings){
        struct pldm_bios_string_table_entry* ptr = reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
        strLen = ptr->string_length;
        ASSERT_EQ(strLen,elem.size());
        ASSERT_EQ(0,memcmp(elem.c_str(),ptr->name,strLen));
        tableData += 2; //pass through the handle
        tableData += 2; //pass through the string length
        tableData += strLen; //pass through the string
        
    }//end for
}


TEST(getBIOSAttributeTable,testGoodRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES> requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req *req = (struct pldm_get_bios_table_req *)request->payload;
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_ATTR_TABLE;

    AttrValuesMap attrMap = getBIOSValues(BIOS_ATTRIBUTE_TABLE_FILE_PATH);

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    auto response = getBIOSTable(request,requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    struct pldm_get_bios_table_resp* resp = reinterpret_cast<struct pldm_get_bios_table_resp*>(responsePtr->payload);

    ASSERT_EQ(0,resp->completion_code);
    ASSERT_EQ(0,resp->next_transfer_handle);
    ASSERT_EQ(PLDM_START_AND_END,resp->transfer_flag);

    //size_t respPayloadLength = response.size() - sizeof(pldm_msg_hdr) - (sizeof(resp->completion_code)+ sizeof(resp->next_transfer_handle) + sizeof(resp->transfer_flag));
    size_t counter = 1;
    uint16_t attrHdl = 0;
    uint16_t stringHdl = 0;
    uint8_t attrType = 0;
    uint8_t numPosVals = 0;
    uint8_t numDefVals = 0;
    uint8_t defIndex = 0;

    uint8_t *tableData = reinterpret_cast<uint8_t*>(resp->table_data);
    //while (1)
    {
        struct pldm_bios_attr_table_entry* ptr = reinterpret_cast<struct pldm_bios_attr_table_entry*>(tableData);
        attrHdl = ptr->attr_handle;
        attrType = ptr->attr_type;
        EXPECT_EQ(0,attrHdl);
        EXPECT_EQ(PLDM_BIOS_ENUMERATION,attrType);
        stringHdl = ptr->string_handle;
        EXPECT_EQ(stringHdl,9);
        tableData += sizeof(attrHdl) + sizeof(attrType) + sizeof(stringHdl);
        counter += sizeof(attrHdl) + sizeof(attrType) + sizeof(stringHdl); //traversed these many bytes in response
        numPosVals = *tableData;
        EXPECT_EQ(numPosVals,3);
        //test
        PossibleValuesByHandle possiVals;
        tableData ++;
        uint16_t* temp = reinterpret_cast<uint16_t*>(tableData);
        uint32_t i = 0;
        while(i<numPosVals){
            uint16_t val = *temp;
            possiVals.push_back(val);
            temp++;
            i++;
        }
        EXPECT_EQ(possiVals.size(),3);
        tableData += numPosVals * sizeof(stringHdl);
        //end test
       // tableData += sizeof(numPosVals) +  numPosVals * sizeof(stringHdl); //pass all the possible vals
        counter += numPosVals * sizeof(stringHdl);
        numDefVals = *tableData;
        EXPECT_EQ(numDefVals,1);
        tableData += numDefVals * sizeof(defIndex); //pass all the default data
        counter += numDefVals * sizeof(defIndex);

      //  if ( counter >= respPayloadLength) //to add check later if respPayloadLength - counter is less than 8 then remaining is pad and checksum
        //    break;

    } //end while

}//end TEST


TEST(getBIOSAttributeValueTable,testGoodRequest)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_REQ_BYTES> requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    struct pldm_get_bios_table_req *req = (struct pldm_get_bios_table_req *)request->payload;
    req->transfer_handle = 9;
    req->transfer_op_flag = PLDM_GET_FIRSTPART;
    req->table_type = PLDM_BIOS_ATTR_VAL_TABLE;

    std::string attrName("DumpPolicy");
    CurrentValues currVals =  getAttrValue(attrName);

    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    auto response = getBIOSTable(request,requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    struct pldm_get_bios_table_resp* resp = reinterpret_cast<struct pldm_get_bios_table_resp*>(responsePtr->payload);

    ASSERT_EQ(0,resp->completion_code);
    ASSERT_EQ(0,resp->next_transfer_handle);
    ASSERT_EQ(PLDM_START_AND_END,resp->transfer_flag);

    uint8_t *tableData = reinterpret_cast<uint8_t*>(resp->table_data);

    uint16_t attrHdl;
    uint8_t attrType;
    uint8_t numCurrVals;
    uint8_t currValStrIndex;

    //while(1)
    struct pldm_bios_attr_val_table_entry* ptr = reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(tableData);
    attrHdl = ptr->attr_handle;
    attrType = ptr->attr_type;
    EXPECT_EQ(0,attrHdl);
    EXPECT_EQ(PLDM_BIOS_ENUMERATION,attrType);
    tableData += sizeof(attrHdl) + sizeof(attrType);
    numCurrVals = *tableData;
    EXPECT_EQ(1,numCurrVals);
    tableData += sizeof(numCurrVals);
    currValStrIndex = *tableData;
    EXPECT_EQ(1,currValStrIndex);
}//end TEST
