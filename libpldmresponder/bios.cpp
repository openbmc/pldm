#include "bios.hpp"
//#include "bios_table.hpp"
//#include "bios_parser.hpp"

#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <array>
#include <chrono>
#include <ctime>
#include <fstream> //for testing
#include <iostream>
#include <phosphor-logging/log.hpp>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
namespace pldm
{

using namespace pldm::responder::bios;
using namespace phosphor::logging;
using EpochTimeUS = uint64_t;
using Row = std::vector<uint8_t>;

constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";

namespace responder
{

namespace utils
{

void epochToBCDTime(uint64_t timeSec, uint8_t& seconds, uint8_t& minutes,
                    uint8_t& hours, uint8_t& day, uint8_t& month,
                    uint16_t& year)
{
    auto t = time_t(timeSec);
    auto time = localtime(&t);

    seconds = decimalToBcd(time->tm_sec);
    minutes = decimalToBcd(time->tm_min);
    hours = decimalToBcd(time->tm_hour);
    day = decimalToBcd(time->tm_mday);
    month =
        decimalToBcd(time->tm_mon + 1); // The number of months in the range
                                        // 0 to 11.PLDM expects range 1 to 12
    year = decimalToBcd(time->tm_year + 1900); // The number of years since 1900
}

} // namespace utils

Response getDateTime(const pldm_msg* request)
{
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    constexpr auto timeInterface = "xyz.openbmc_project.Time.EpochTime";
    constexpr auto bmcTimePath = "/xyz/openbmc_project/time/bmc";
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_DATE_TIME_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    std::variant<EpochTimeUS> value;

    auto bus = sdbusplus::bus::new_default();
    try
    {
        auto service = getService(bus, bmcTimePath, timeInterface);

        auto method = bus.new_method_call(service.c_str(), bmcTimePath,
                                          dbusProperties, "Get");
        method.append(timeInterface, "Elapsed");

        auto reply = bus.call(method);
        reply.read(value);
    }

    catch (std::exception& e)
    {
        log<level::ERR>("Error getting time", entry("PATH=%s", bmcTimePath),
                        entry("TIME INTERACE=%s", timeInterface));

        encode_get_date_time_resp(0, PLDM_ERROR, seconds, minutes, hours, day,
                                  month, year, responsePtr);
        return response;
    }

    uint64_t timeUsec = std::get<EpochTimeUS>(value);

    uint64_t timeSec = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::microseconds(timeUsec))
                           .count();

    utils::epochToBCDTime(timeSec, seconds, minutes, hours, day, month, year);

    encode_get_date_time_resp(0, PLDM_SUCCESS, seconds, minutes, hours, day,
                              month, year, responsePtr);
    return response;
}

Response getBIOSTable(const pldm_msg* request, size_t payloadLength)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    uint32_t transferHandle{};
    uint8_t transferOpFlag{};
    uint8_t tableType{};
    uint32_t nxtTransferHandle{};
    uint8_t transferFlag{};
    size_t respPayloadLength{};

    static bios::BIOSTable BIOSStringTable(BIOS_STRING_TABLE_FILE_PATH);
    static bios::BIOSTable BIOSAttributeTable(BIOS_ATTRIBUTE_TABLE_FILE_PATH);
    static bios::BIOSTable BIOSAttributeValueTable(
        BIOS_ATTRIBUTE_VALUE_TABLE_PATH);

    auto rc =
        decode_get_bios_table_req(request->payload, payloadLength,
                                  &transferHandle, &transferOpFlag, &tableType);

    if (rc == PLDM_SUCCESS)
    {
        switch (tableType)
        {
            case PLDM_BIOS_STRING_TABLE:
                response = getBIOSStringTable(BIOSStringTable, transferHandle,
                                              transferOpFlag);
                break;
            case PLDM_BIOS_ATTR_TABLE: // after a bmc reboot all the tables will
                                       // be empty but can phyp call attr first
                                       // that time? how to handle that case?
                if (BIOSStringTable.isEmpty())
                {
                    rc = PLDM_BIOS_TABLE_UNAVAILABLE;
                }
                else
                {
                    response = getBIOSAttributeTable(
                        BIOSAttributeTable, BIOSStringTable, transferHandle,
                        transferOpFlag);
                }
                break;
            case PLDM_BIOS_ATTR_VAL_TABLE:
                if (BIOSAttributeTable.isEmpty())
                {
                    rc = PLDM_BIOS_TABLE_UNAVAILABLE;
                }
                else
                {
                    response = getBIOSAttributeValueTable(
                        BIOSAttributeValueTable, BIOSAttributeTable,
                        BIOSStringTable, transferHandle, transferOpFlag);
                }
                break;
            default:
                rc = PLDM_INVALID_BIOS_TABLE_TYPE;
                break;
        }
    } // end if

    if (rc != PLDM_SUCCESS)
    {
        encode_get_bios_table_resp(0, rc, nxtTransferHandle, transferFlag,
                                   nullptr, respPayloadLength, responsePtr);
    }

    return response;
} // end getBIOSTable

Response getBIOSStringTable(bios::BIOSTable& BIOSStringTable,
                            uint32_t& transferHandle, uint8_t& transferOpFlag)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;
    size_t respPayloadLength{};
    // to be added at the end of the table PAD and CHECKSUM

    if (BIOSStringTable.isEmpty())
    { // no persisted table, no cache, constructing fresh table cache and data
        Strings biosStrings = getBIOSStrings(BIOS_STRING_TABLE_FILE_PATH);
        bios::Table stringTable;
        for (auto& elem : biosStrings)
        {
            Row entryToStringTable(sizeof(uint16_t) * 2 + elem.size(), 0);
            struct pldm_bios_string_table_entry* stringPtr =
                reinterpret_cast<struct pldm_bios_string_table_entry*>(
                    entryToStringTable.data());
            stringPtr->string_handle = nextStringHandle();
            stringPtr->string_length = elem.length();
            memcpy(stringPtr->name, elem.c_str(), elem.length());
            stringTable.insert(stringTable.end(), entryToStringTable.begin(),
                               entryToStringTable.end());
        } // end for
        BIOSStringTable.store(stringTable);

        response.resize(sizeof(pldm_msg_hdr) +
                            PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                            stringTable.size(),
                        0);
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        respPayloadLength = response.size();
        encode_get_bios_table_resp(0, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, stringTable.data(),
                                   respPayloadLength, responsePtr);
        Response newResponse(response); // this is for test
        // std::cout<<"newResponse.size()" << newResponse.size() << endl;
        // std::cout << "response.size()" << response.size() << endl;
        BIOSStringTable.setResponse(std::move(response));
        // return newResponse; //this is for test
    }
    else if (!BIOSStringTable.hasResponse())
    { // only persisted table present, constructing a cache and data
        respPayloadLength = response.size();
        encode_get_bios_table_resp(0, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr); // filling up the header here
        BIOSStringTable.load(response);
        BIOSStringTable.setResponse(std::move(response));
    }

    Response newResponse1 =
        BIOSStringTable.getResponse(); // get data from cache and send
    // std::cout<<"newResponse1.size()" << newResponse1.size() << endl;
    return newResponse1;
}

Response getBIOSAttributeTable(bios::BIOSTable& BIOSAttributeTable,
                               bios::BIOSTable& BIOSStringTable,
                               uint32_t& transferHandle,
                               uint8_t& transferOpFlag)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;
    size_t respPayloadLength{};
    // to be added at the end of the attr table the pad and checksum
    //
    // check if BIOSStringTable is empty else call getBIOSStringTable to
    // construct the string table

    if (BIOSAttributeTable.isEmpty())
    { // no persisted table, no cache, constructing fresh table cache and data
        std::cout << "BIOSAttributeTable is empty calling "
                     "getBIOSAttributeEnumerationTable"
                  << endl;
        bios::Table attributeTable =
            getBIOSAttributeEnumerationTable(BIOSStringTable);
        std::cout << "got size of attributeTable " << attributeTable.size()
                  << endl;
        BIOSAttributeTable.store(attributeTable);
        response.resize(sizeof(pldm_msg_hdr) +
                        PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                        attributeTable.size());
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        respPayloadLength = response.size();
        encode_get_bios_table_resp(0, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, attributeTable.data(),
                                   respPayloadLength, responsePtr);
        Response newResponse(response); // this is for test
        BIOSAttributeTable.setResponse(std::move(response));
        return newResponse; // this is for test
    }                       // end if
    else if (!BIOSAttributeTable.hasResponse())
    { // only persisted table present, constructing a cache and data
        respPayloadLength = response.size();
        encode_get_bios_table_resp(0, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr); // filling up the header here
        BIOSAttributeTable.load(response);
        BIOSAttributeTable.setResponse(std::move(response));
    } // end else

    Response newResponse =
        BIOSAttributeTable.getResponse(); // get data from cache and send
    return newResponse;
} // end getBIOSAttributeTable

Response getBIOSAttributeValueTable(bios::BIOSTable& BIOSAttributeValueTable,
                                    bios::BIOSTable& BIOSAttributeTable,
                                    bios::BIOSTable& BIOSStringTable,
                                    uint32_t& transferHandle,
                                    uint8_t& transferOpFlag)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;
    size_t respPayloadLength{};
    // to be added at the end of the attr value table the pad and checksum
    //
    if (BIOSAttributeValueTable.isEmpty())
    { // no persisted table, no cache, constructing fresh table cache and data
        std::cout << "attribute value table empty, calling "
                     "getBIOSAttributeValueEnumerationTable"
                  << endl;
        bios::Table attributeValueTable = getBIOSAttributeValueEnumerationTable(
            BIOSAttributeTable, BIOSStringTable);
        std::cout << "after getBIOSAttributeValueEnumerationTable, got size of "
                     "attr val table "
                  << attributeValueTable.size() << endl;
        BIOSAttributeValueTable.store(attributeValueTable);
        response.resize(sizeof(pldm_msg_hdr) +
                        PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                        attributeValueTable.size());
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        respPayloadLength = response.size();
        encode_get_bios_table_resp(0, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, attributeValueTable.data(),
                                   respPayloadLength, responsePtr);
        Response newResponse(response); // this is for test
        BIOSAttributeValueTable.setResponse(std::move(response));
        return newResponse; // this is for test
    }
    else if (!BIOSAttributeValueTable.hasResponse())
    { // only persisted table present, constructing a cache and data
        respPayloadLength = response.size();
        encode_get_bios_table_resp(0, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr); // filling up the header here
        BIOSAttributeValueTable.load(response);
        BIOSAttributeValueTable.setResponse(std::move(response));
    } // end else if

    Response newResponse =
        BIOSAttributeValueTable.getResponse(); // get data from cache and send
    return newResponse;

} // end getBIOSAttributeValueTable

Strings getBIOSStrings(const char* dirPath) // to be removed after Tom's patch
{
    Strings biosStrings = {"This",      "On",        "Off",     "HMCManaged",
                           "Enabled",   "Disabled",  "IPLSide", "USBEmulation",
                           "Persisted", "DumpPolicy"};
    return biosStrings;
}

AttrValuesMap
    getBIOSValues(const char* dirPath) // to be removed after Tom's patch
{
    AttrValuesMap attrMap;
    std::tuple<PossibleValues, DefaultValues> tp;
    /*PossibleValues pos = {"Persisted","This"};
    DefaultValues def = {"This"};
    tp = std::make_tuple(pos,def);
attrMap["HMCManaged"] = tp;

pos = {"Off","On"};
def = {"Off"};
tp = std::make_tuple(pos,def);
attrMap["IPLSide"] = tp;*/

    PossibleValues pos = {"Persisted", "Enabled", "Disabled"};
    DefaultValues def = {"Disabled"};
    tp = std::make_tuple(pos, def);
    attrMap["DumpPolicy"] = tp;

    return attrMap;
}
CurrentValues
    getAttrValue(const std::string& attrName) // to be removed after Tom's patch
{
    // considering attribute name("DumpPolicy");
    CurrentValues currVals;
    currVals.push_back("Enabled");
    return currVals;
} // end getAttrValue

handle nextStringHandle()
{
    static handle hdl = 0;
    return hdl++;
}

handle nextAttrHandle()
{
    static handle hdl = 0;
    return hdl++;
}

handle findStringHandle(std::string name, bios::BIOSTable& BIOSStringTable)
{
    // std::cout<<"entered first findStringHandle calling getResponse" << endl;
    handle hdl = 0xFF;
    Response response = BIOSStringTable.getResponse();
    // std::cout<<"after getResponse" << endl;
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);

    //   std::cout<<"getResponse size" << response.size() << endl;

    uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
    struct pldm_bios_string_table_entry* ptr =
        reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    while (tableData != NULL)
    {
        handle temp = ptr->string_handle;
        uint16_t len = ptr->string_length;
        if (memcmp(name.c_str(), ptr->name, len) == 0)
        {
            hdl = temp;
            break;
        }
        tableData += 2;   // pass through the handle
        tableData += 2;   // pass through the string length
        tableData += len; // pass through the string
        ptr = reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    }
    return hdl;
} // end findStringHandle

std::string findStringName(handle stringHandle,
                           bios::BIOSTable& BIOSStringTable)
{
    std::string name;
    Response response = BIOSStringTable.getResponse();
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);

    std::cout << "inside findStringName with handle " << stringHandle << endl;

    uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
    struct pldm_bios_string_table_entry* ptr =
        reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    while (tableData != NULL)
    {
        std::cout << "inside while loop " << endl;
        handle temp = ptr->string_handle;
        std::cout << "fetched handle from string table " << temp << endl;
        uint16_t len = ptr->string_length;
        std::cout << "fetched length of string from string table " << len;
        if (temp == stringHandle)
        {
            std::cout << "inside if block" << endl;
            name.resize(len);
            std::cout << "after string resize" << endl;
            memcpy(name.data(), ptr->name, len);
            std::cout << "after memcpy " << endl;
            break;
        } // end if
        tableData += sizeof(handle) + sizeof(len) + len;
        ptr = reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    } // end while

    return name;

} // end findStringName

std::vector<uint8_t> findStrIndices(PossibleValuesByHandle possiVals,
                                    CurrentValues currVals,
                                    bios::BIOSTable& BIOSStringTable)
{
    std::vector<uint8_t> stringIndices(
        currVals.size()); // push_back crashing without the size mentioned
    for (auto& currVal : currVals)
    {
        handle curHdl = findStringHandle(currVal, BIOSStringTable);
        uint8_t i = 0;
        for (auto possiHdl : possiVals)
        {
            if (possiHdl == curHdl)
            {
                stringIndices.push_back(i);
                uint32_t j = i;
                std::cout << "after stringIndices.push_back index  " << j
                          << endl;
                break;
            }
            i++;
        } // end inner for
    }     // end for
    std::cout << "returning from findStrIndices " << endl;
    return stringIndices; // we never reach to the caller
} // end findStrIndices

std::vector<handle> findStringHandle(std::vector<std::string> values,
                                     bios::BIOSTable& BIOSStringTable)
{
    std::vector<handle> valsByHdl;
    handle def = 0xFF; // some invalid value
    Response response = BIOSStringTable.getResponse();
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);

    for (auto& name : values)
    {
        uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
        struct pldm_bios_string_table_entry* ptr =
            reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
        uint16_t len{};
        bool found = false;
        while (tableData != NULL)
        {
            handle temp = ptr->string_handle;
            len = ptr->string_length;
            if (memcmp(name.c_str(), ptr->name, len) == 0)
            {
                valsByHdl.push_back(temp);
                found = true;
                break;
            }
            tableData += 2;   // pass through the handle
            tableData += 2;   // pass through the string length
            tableData += len; // pass through the string
            ptr = reinterpret_cast<struct pldm_bios_string_table_entry*>(
                tableData);
        }
        if (!found)
        {
            valsByHdl.push_back(def);
        }
    } // end for
    return valsByHdl;

} // end findStringHandle

std::vector<uint8_t> findDefaultValHandle(PossibleValues possiVals,
                                          DefaultValues defVals)
{
    std::vector<uint8_t> defHdls;
    uint8_t i = 0;
    uint32_t j;
    j = i;
    std::cout << "finding default values indices set i as " << j;
    for (auto& defs : defVals)
    {
        std::cout << "\n for default val " << defs << endl;
        i = 0;
        for (auto& possi : possiVals)
        {
            j = i;
            std::cout << "\npicked possi " << possi << " and index is " << j;
            if (possi.compare(defs) == 0)
            {
                defHdls.push_back(i);
                std::cout << "\nfound def in possi, setting index" << j << endl;
                break;
            }
            i++;
        } // end inner for
    }     // end outer for

    return defHdls;
} // end findDefaultValHandle

bios::Table getBIOSAttributeEnumerationTable(bios::BIOSTable& BIOSStringTable)
{
    AttrValuesMap attributeMap = getBIOSValues(BIOS_ATTRIBUTE_TABLE_FILE_PATH);
    AttrValuesMapByHandle attrMapByHdl;
    PossibleValuesByHandle possibleValByHdl;
    DefaultValuesByHandle defaultValByHdl;
    bios::Table attributeTable;

    std::cout << "inside getBIOSAttributeEnumerationTable" << endl;

    for (auto& [key, value] : attributeMap)
    {
        std::cout << endl;
        std::cout << "picked attribute " << key << endl;
        handle attrNameHdl = findStringHandle(key, BIOSStringTable);
        std::cout << "got handle " << attrNameHdl
                  << " from string table for the attribute" << endl; // correct
        PossibleValues possiVals = std::get<0>(value);               // correct
        std::cout << "possible values for the attribute: " << endl;
        for (auto& e : possiVals)
        {
            std::cout << e << " ";
        }
        std::cout << endl;
        DefaultValues defVals = std::get<1>(value); // correct
        std::cout << "default values for the attribute: " << endl;
        for (auto& e : defVals)
        {
            std::cout << e << " ";
        }
        std::cout << endl;

        auto possiValsByHdl = findStringHandle(possiVals, BIOSStringTable);
        std::cout << "handles for the possible values from the string table"
                  << endl; 
        for (auto& e : possiValsByHdl)
        {
            std::cout << " " << e << " ";
        }
        std::cout << endl;
        std::cout << "possiValsByHdl.size() " << possiValsByHdl.size() << endl;
        auto defValsByHdl =
            findDefaultValHandle(possiVals, defVals); 
        std::cout
            << "indices for the default values from the possible values is"
            << endl;
        for (auto& e : defValsByHdl)
        {
            uint32_t j = e;
            std::cout << " " << j << " ";
        }
        std::cout << endl;
        std::cout << "defValsByHdl.size() " << defValsByHdl.size() << endl;

        std::cout << "deciding size of the row of attribute table "
                  << sizeof(uint16_t) << " + " << sizeof(uint8_t);
        std::cout << " + " << sizeof(uint16_t) << " + " << sizeof(uint8_t);
        std::cout << " + " << possiValsByHdl.size() * sizeof(uint16_t) << " + ";
        std::cout << sizeof(uint8_t) << " + "
                  << defValsByHdl.size() * sizeof(uint8_t) << endl;
        Row entryToAttrTable(
            sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t) +
                sizeof(uint8_t) + possiValsByHdl.size() * sizeof(uint16_t) +
                sizeof(uint8_t) + defValsByHdl.size() * sizeof(uint8_t),
            0);
        std::cout << "entryToAttrTable.size() " << entryToAttrTable.size()
                  << endl;
        struct pldm_bios_attr_table_entry* attrPtr =
            reinterpret_cast<struct pldm_bios_attr_table_entry*>(
                entryToAttrTable.data());
        attrPtr->attr_handle = nextAttrHandle();
        std::cout << "generated attribute handle " << attrPtr->attr_handle
                  << endl;
        attrPtr->attr_type = PLDM_BIOS_ENUMERATION;
        uint32_t j = attrPtr->attr_type;

        std::cout << "set attribute type as " << j << endl;
        attrPtr->string_handle = attrNameHdl;
        std::cout << " set attribute string handle " << attrPtr->string_handle
                  << endl;
        uint8_t* metaData = reinterpret_cast<uint8_t*>(attrPtr->metadata);
        *metaData = possiValsByHdl.size();
        j = *metaData;
        std::cout << "number of possible vals " << j << endl;
        metaData++;
        uint16_t* metaDataVal = reinterpret_cast<uint16_t*>(metaData);
        for (auto& elem : possiValsByHdl)
        {
            *metaDataVal = elem;
            std::cout << "possible vals handles " << *metaDataVal << endl;
            metaDataVal++;
        } // end for
        metaData = reinterpret_cast<uint8_t*>(metaDataVal);
        *metaData = defValsByHdl.size();
        j = *metaData;
        std::cout << "number of default values " << j << endl;
        metaData++;
        for (auto& elem : defValsByHdl)
        {
            *metaData = elem;
            j = *metaData;
            std::cout << "default values handle " << j << endl;
            metaData++;
        } // end for

        std::cout << "printing the entire Row for attribute table" << endl;
        for (auto& e : entryToAttrTable)
        {
            uint32_t j = e;
            std::cout << j << endl;
        }
        attributeTable.insert(attributeTable.end(), entryToAttrTable.begin(),
                              entryToAttrTable.end());
    } // end outer for

    std::cout << "generated the total size of attributeTable "
              << attributeTable.size() << endl;

    return attributeTable;
}

// CHECK ALL THE FUNCTIONS LIKE FINDSTRINGHANDLE
// TO CONSIDER THE CHECKSUM AND PAD WHILE TRAVERSING ANY TABLE

bios::Table
    getBIOSAttributeValueEnumerationTable(bios::BIOSTable& BIOSAttributeTable,
                                          bios::BIOSTable& BIOSStringTable)
{
    bios::Table attributeValueTable;
    Response attributeResponse = BIOSAttributeTable.getResponse();
    auto attrResponsePtr =
        reinterpret_cast<pldm_msg*>(attributeResponse.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            attrResponsePtr->payload);

    uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
    struct pldm_bios_attr_table_entry* attrPtr =
        reinterpret_cast<struct pldm_bios_attr_table_entry*>(tableData);

    uint16_t attrHdl;
    uint16_t stringHdl;
    uint8_t attrType;
    uint8_t numCurrVals;
    std::cout << "inside getBIOSAttributeValueEnumerationTable" << endl;
    uint16_t* test;
    size_t respPayloadLength =
        attributeResponse.size() - sizeof(pldm_msg_hdr) -
        (sizeof(resp->completion_code) + sizeof(resp->next_transfer_handle) +
         sizeof(resp->transfer_flag));
    std::cout << "response table size= " << respPayloadLength << endl;
    // while() {  //add a check for pad and checksum
    test = reinterpret_cast<uint16_t*>(tableData);
    std::cout << "inside while loop tableData= " << test
              << " attrPtr = " << attrPtr << endl;
    attrHdl = attrPtr->attr_handle;
    attrType = attrPtr->attr_type;
    stringHdl = attrPtr->string_handle;
    std::cout << "fetched attribute handle " << attrHdl << endl;
    std::cout << "fetched string handle " << stringHdl << endl;
    tableData += sizeof(attrHdl) + sizeof(attrType) + sizeof(stringHdl);
    test = reinterpret_cast<uint16_t*>(tableData);
    std::cout << "after pointing to metaData, tableData= " << test << endl;
    uint8_t numPossiVals = *tableData;
    uint32_t j = numPossiVals;
    std::cout << "number of possi vals " << j << endl;
    tableData++; // pass number of possible values
    test = reinterpret_cast<uint16_t*>(tableData);
    std::cout << "after numPossiVals, tableData= " << test << endl;
    uint16_t* temp;
    temp = reinterpret_cast<uint16_t*>(tableData);
    std::cout << "temp= " << temp << endl;
    PossibleValuesByHandle possiVals;
    possiVals.clear();
    possiVals.reserve(
        numPossiVals); // without this push_back was crashing at 3rd element
    uint32_t i = 0;
    uint16_t val;
    while (i < numPossiVals)
    {
        val = *temp;
        std::cout << "possible value handle " << val << endl;
        possiVals.push_back(val);
        std::cout << "after possiVals.push_back i= " << i << endl;
        temp++;
        std::cout << "after " << i << "th val, temp= " << temp << endl;
        i++;
    }
    for (auto& e : possiVals)
    {
        std::cout << "possi val handle " << e << endl;
    }
    tableData = reinterpret_cast<uint8_t*>(temp);
    uint8_t numDefVals = *tableData;
    j = numDefVals;
    std::cout << "number of def vals " << j << endl;
    tableData++;             // pass number of def vals
    tableData += numDefVals; // pass all the def val indices

    std::cout << "default val index " << *tableData << endl;
    std::cout << "calling findStringName with handle " << stringHdl << endl;
    std::string attrName = findStringName(stringHdl, BIOSStringTable);
    std::cout << "fetched attribute name from string table " << attrName
              << endl;
    CurrentValues currVals = getAttrValue(attrName);
    numCurrVals = currVals.size();
    std::cout << "fetched all the current values for the attribute, size "
              << currVals.size() << endl;
    auto currValStrIndices =
        findStrIndices(possiVals, currVals, BIOSStringTable);
    std::cout << "after findStrIndices currValStrIndices.size()"
              << currValStrIndices.size() << endl;

    Row entryToAttrValTable(sizeof(uint16_t) + sizeof(uint8_t) +
                                sizeof(uint8_t) + numCurrVals * sizeof(uint8_t),
                            0);
    struct pldm_bios_attr_val_table_entry* attrValPtr =
        reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(
            entryToAttrValTable.data());
    attrValPtr->attr_handle = attrHdl;
    attrValPtr->attr_type = attrType;
    uint8_t* metaData = reinterpret_cast<uint8_t*>(attrValPtr->value);
    *metaData = numCurrVals;
    metaData++;
    for (auto& elem : currValStrIndices)
    {
        *metaData = elem;
        metaData++;
    } // end for
    attributeValueTable.insert(attributeValueTable.end(),
                               entryToAttrValTable.begin(),
                               entryToAttrValTable.end());

    attrPtr = reinterpret_cast<struct pldm_bios_attr_table_entry*>(tableData);
    // add a check here to see the pad and checksum

    //}//end while

    std::cout << "returning from getBIOSAttributeValueEnumerationTable" << endl;

    return attributeValueTable;
} // end getBIOSAttributeValueEnumerationTable

} // namespace responder
} // namespace pldm
