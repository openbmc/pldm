#include "bios.hpp"

#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <array>
#include <boost/crc.hpp>
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
    std::string filePath(ALL_BIOS_DIR);

    auto response = buildBIOSTables(request, payloadLength, filePath.c_str());

    return response;
}

Response buildBIOSTables(const pldm_msg* request, size_t payloadLength,
                         const char* path)
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
    uint8_t instanceID = request->hdr.instance_id;

    std::string strFile;
    std::string attrFile;
    std::string attrValFile;

    strFile.append(path);
    strFile.append(BIOS_STRING_TABLE_FILE_PATH);
    attrFile.append(path);
    attrFile.append(BIOS_ATTRIBUTE_TABLE_FILE_PATH);
    attrValFile.append(path);
    attrValFile.append(BIOS_ATTRIBUTE_VALUE_TABLE_PATH);

    bios::BIOSTable BIOSStringTable(strFile.c_str());
    bios::BIOSTable BIOSAttributeTable(attrFile.c_str());
    bios::BIOSTable BIOSAttributeValueTable(attrValFile.c_str());

    auto rc = decode_get_bios_table_req(request, payloadLength, &transferHandle,
                                        &transferOpFlag, &tableType);

    if (rc == PLDM_SUCCESS)
    {
        switch (tableType)
        {
            case PLDM_BIOS_STRING_TABLE:
                response = getBIOSStringTable(BIOSStringTable, transferHandle,
                                              transferOpFlag, instanceID, path);
                break;
            case PLDM_BIOS_ATTR_TABLE:

                if (BIOSStringTable.isEmpty())
                {
                    rc = PLDM_BIOS_TABLE_UNAVAILABLE;
                }
                else
                {
                    response = getBIOSAttributeTable(
                        BIOSAttributeTable, BIOSStringTable, transferHandle,
                        transferOpFlag, instanceID, path);
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
                        BIOSStringTable, transferHandle, transferOpFlag,
                        instanceID, path);
                }
                break;
            default:
                rc = PLDM_INVALID_BIOS_TABLE_TYPE;
                break;
        }
    } // end if

    if (rc != PLDM_SUCCESS)
    {
        encode_get_bios_table_resp(instanceID, rc, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr);
    }

    return response;
} // end getBIOSTable

Response getBIOSStringTable(bios::BIOSTable& BIOSStringTable,
                            uint32_t& transferHandle, uint8_t& transferOpFlag,
                            uint8_t instanceID, const char* path)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;
    size_t respPayloadLength{};

    if (BIOSStringTable.isEmpty())
    { // no persisted table, constructing fresh table and file
        Strings biosStrings = getBIOSStrings(path);
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
        std::cout << "string table length without pad " << stringTable.size()
                  << endl;
        uint8_t padSize = calculatePad(stringTable.size());
        std::vector<uint8_t> pad(padSize, 0);
        if (padSize)
        {
            stringTable.insert(stringTable.end(), pad.begin(), pad.end());
        }
        // compute checksum
        boost::crc_32_type result;
        result.process_bytes(stringTable.data(), stringTable.size());
        uint32_t checkSum = result.checksum();
        std::cout << "calculated checksum" << checkSum << endl;
        uint32_t size = stringTable.size();
        stringTable.resize(size + sizeof(checkSum));
        uint8_t* ck = reinterpret_cast<uint8_t*>(&checkSum);
        std::copy(ck, ck + sizeof(checkSum), stringTable.data() + size);
        std::cout << "after pad and check sum, string table length "
                  << stringTable.size() << endl;

        BIOSStringTable.store(stringTable);

        response.resize(sizeof(pldm_msg_hdr) +
                            PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                            stringTable.size(),
                        0);
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, stringTable.data(),
                                   respPayloadLength, responsePtr);
        return response;
    }
    else
    { // only persisted table present, constructing data
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr); // filling up the header here
        BIOSStringTable.load(response);
        return response;
    }

    encode_get_bios_table_resp(instanceID, PLDM_ERROR, nxtTransferHandle,
                               transferFlag, nullptr, respPayloadLength,
                               responsePtr);
    return response;
}

Response getBIOSAttributeTable(bios::BIOSTable& BIOSAttributeTable,
                               bios::BIOSTable& BIOSStringTable,
                               uint32_t& transferHandle,
                               uint8_t& transferOpFlag, uint8_t instanceID,
                               const char* path)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;
    size_t respPayloadLength{};

    // load the string table
    uint32_t dummytransferHandle;
    uint8_t dummytransferOpFlag;
    Response dummyResponse =
        getBIOSStringTable(BIOSStringTable, dummytransferHandle,
                           dummytransferOpFlag, instanceID, path);

    if (BIOSAttributeTable.isEmpty())
    { // no persisted table, constructing fresh table and data
        std::cout << "BIOSAttributeTable is empty calling "
                     "getBIOSAttributeEnumerationTable"
                  << endl;
        bios::Table attributeTable =
            getBIOSAttributeEnumerationTable(BIOSStringTable, path);
        std::cout << "got size of attributeTable " << attributeTable.size()
                  << endl;

        // calculate pad
        uint8_t padSize = calculatePad(attributeTable.size());
        std::vector<uint8_t> pad(padSize, 0);
        if (padSize)
        {
            attributeTable.insert(attributeTable.end(), pad.begin(), pad.end());
            std::cout << "added pad os size " << padSize
                      << " to attribute table" << endl;
        }

        // compute checksum
        boost::crc_32_type result;
        result.process_bytes(attributeTable.data(), attributeTable.size());
        uint32_t checkSum = result.checksum();
        std::cout << "calculated checksum" << checkSum << endl;
        uint32_t size = attributeTable.size();
        attributeTable.resize(size + sizeof(checkSum));
        uint8_t* ck = reinterpret_cast<uint8_t*>(&checkSum);
        std::copy(ck, ck + sizeof(checkSum), attributeTable.data() + size);
        std::cout << "after pad and check sum, attribute table length "
                  << attributeTable.size() << endl;

        BIOSAttributeTable.store(attributeTable);
        response.resize(sizeof(pldm_msg_hdr) +
                        PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                        attributeTable.size());
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, attributeTable.data(),
                                   respPayloadLength, responsePtr);
        return response;
    } // end if
    else
    { // only persisted table present, constructing data
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr); // filling up the header here
        BIOSAttributeTable.load(response);
        return response;
    } // end else

    encode_get_bios_table_resp(instanceID, PLDM_ERROR, nxtTransferHandle,
                               transferFlag, nullptr, respPayloadLength,
                               responsePtr);
    return response;
} // end getBIOSAttributeTable

Response getBIOSAttributeValueTable(bios::BIOSTable& BIOSAttributeValueTable,
                                    bios::BIOSTable& BIOSAttributeTable,
                                    bios::BIOSTable& BIOSStringTable,
                                    uint32_t& transferHandle,
                                    uint8_t& transferOpFlag, uint8_t instanceID,
                                    const char* path)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;
    size_t respPayloadLength{};

    // load attribute table and string table
    uint32_t dummytransferHandle;
    uint8_t dummytransferOpFlag;
    Response dummyResponse = getBIOSAttributeTable(
        BIOSAttributeTable, BIOSStringTable, dummytransferHandle,
        dummytransferOpFlag, instanceID, path);

    if (BIOSAttributeValueTable.isEmpty())
    { // no persisted table, constructing fresh table and data
        std::cout << "attribute value table empty, calling "
                     "getBIOSAttributeValueEnumerationTable"
                  << endl;
        bios::Table attributeValueTable = getBIOSAttributeValueEnumerationTable(
            BIOSAttributeTable, BIOSStringTable);
        std::cout << "after getBIOSAttributeValueEnumerationTable, got size of "
                     "attr val table "
                  << attributeValueTable.size() << endl;
        // calculate pad
        uint8_t padSize = calculatePad(attributeValueTable.size());
        std::vector<uint8_t> pad(padSize, 0);
        if (padSize)
        {
            attributeValueTable.insert(attributeValueTable.end(), pad.begin(),
                                       pad.end());
            std::cout << "added pad of size " << padSize
                      << " to attribute value table" << endl;
        }
        std::cout << "after pad size of value table "
                  << attributeValueTable.size() << endl;

        // compute checksum
        boost::crc_32_type result;
        result.process_bytes(attributeValueTable.data(),
                             attributeValueTable.size());
        uint32_t checkSum = result.checksum();
        std::cout << "calculated checksum" << checkSum << endl;
        uint32_t size = attributeValueTable.size();
        attributeValueTable.resize(size + sizeof(checkSum));
        uint8_t* ck = reinterpret_cast<uint8_t*>(&checkSum);
        std::copy(ck, ck + sizeof(checkSum), attributeValueTable.data() + size);
        std::cout << "after pad and check sum, attribute value table length "
                  << attributeValueTable.size() << endl;

        BIOSAttributeValueTable.store(attributeValueTable);

        response.resize(sizeof(pldm_msg_hdr) +
                        PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                        attributeValueTable.size());
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, attributeValueTable.data(),
                                   respPayloadLength, responsePtr);
        return response;
    }
    else
    { // only persisted table present, constructing data
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr); // filling up the header here
        BIOSAttributeValueTable.load(response);
        return response;
    } // end else

    encode_get_bios_table_resp(instanceID, PLDM_ERROR, nxtTransferHandle,
                               transferFlag, nullptr, respPayloadLength,
                               responsePtr);
    return response;

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
    std::tuple<ReadOnly, PossibleValues, DefaultValues> tp;
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
    tp = std::make_tuple(false, pos, def);
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
    std::cout << "entered first findStringHandle with name " << name.c_str()
              << endl;
    handle hdl{};
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    size_t respPayloadLength = response.size();
    encode_get_bios_table_resp(0, PLDM_SUCCESS, 0, PLDM_START_AND_END, nullptr,
                               respPayloadLength,
                               responsePtr); // filling up the header here
    BIOSStringTable.load(response);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);

    std::cout << "getResponse size" << response.size() << endl;

    uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
    struct pldm_bios_string_table_entry* ptr =
        reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    uint32_t tableLen =
        response.size() - sizeof(pldm_msg_hdr) -
        (sizeof(resp->completion_code) + sizeof(resp->next_transfer_handle) +
         sizeof(resp->transfer_flag));
    std::cout << "length of string table " << tableLen << endl;
    uint32_t traversed = 0;
    while (1)
    {
        handle temp = ptr->string_handle;
        uint16_t len = ptr->string_length;
        if (memcmp(name.c_str(), ptr->name, len) == 0)
        {
            std::cout << "inside memcmp" << endl;
            hdl = temp;
            std::cout << "after hdl=temp" << endl;
            break;
        }
        tableData += sizeof(handle) + sizeof(len) + len;
        traversed += sizeof(handle) + sizeof(len) + len;

        if (traversed > tableLen)
        {
            log<level::ERR>("Error in findStringHandle",
                            entry("STRING=%s", name.c_str()));
            break;
        }
        else if ((tableLen - traversed) < 8)
        {
            std::cout << "reached pad, exiting string table" << endl;
            break;
        }

        ptr = reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    }
    std::cout << "outside while loop" << endl;
    return hdl;
} // end findStringHandle

std::string findStringName(handle stringHandle,
                           bios::BIOSTable& BIOSStringTable)
{
    std::string name;
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    size_t respPayloadLength = response.size();
    encode_get_bios_table_resp(0, PLDM_SUCCESS, 0, PLDM_START_AND_END, nullptr,
                               respPayloadLength,
                               responsePtr); // filling up the header here
    BIOSStringTable.load(response);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);

    std::cout << "inside findStringName with handle " << stringHandle << endl;

    uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
    struct pldm_bios_string_table_entry* ptr =
        reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    uint32_t tableLen =
        response.size() - sizeof(pldm_msg_hdr) -
        (sizeof(resp->completion_code) + sizeof(resp->next_transfer_handle) +
         sizeof(resp->transfer_flag));
    uint32_t traversed = 0;
    while (1)
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
        traversed += sizeof(handle) + sizeof(len) + len;

        if (traversed > tableLen)
        {
            log<level::ERR>("Error in findStringName",
                            entry("HANDLE=0x%x", stringHandle));
            break;
        }
        else if ((tableLen - traversed) < 8)
        {
            std::cout << "reached pad, exiting string table" << endl;
            break;
        }

        ptr = reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    } // end while

    return name;

} // end findStringName

std::vector<uint8_t> findStrIndices(PossibleValuesByHandle possiVals,
                                    CurrentValues currVals,
                                    bios::BIOSTable& BIOSStringTable)
{
    std::vector<uint8_t> stringIndices;

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
    return stringIndices;
} // end findStrIndices

std::vector<handle> findStringHandle(std::vector<std::string> values,
                                     bios::BIOSTable& BIOSStringTable)
{
    std::vector<handle> valsByHdl;
    handle def = 0xFF;
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    size_t respPayloadLength = response.size();
    encode_get_bios_table_resp(0, PLDM_SUCCESS, 0, PLDM_START_AND_END, nullptr,
                               respPayloadLength,
                               responsePtr); // filling up the header here
    BIOSStringTable.load(response);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);
    uint32_t tableLen =
        response.size() - sizeof(pldm_msg_hdr) -
        (sizeof(resp->completion_code) + sizeof(resp->next_transfer_handle) +
         sizeof(resp->transfer_flag));

    for (auto& name : values)
    {
        uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
        struct pldm_bios_string_table_entry* ptr =
            reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
        uint16_t len{};
        bool found = false;
        uint32_t traversed = 0;
        while (1)
        {
            handle temp = ptr->string_handle;
            len = ptr->string_length;
            if (memcmp(name.c_str(), ptr->name, len) == 0)
            {
                valsByHdl.push_back(temp);
                found = true;
                break;
            }
            tableData += sizeof(handle) + sizeof(len) + len;
            traversed += sizeof(handle) + sizeof(len) + len;

            if (traversed > tableLen)
            {
                log<level::ERR>("Error in findStringHandle",
                                entry("STRING=%s", name.c_str()));
                break;
            }
            else if ((tableLen - traversed) < 8)
            {
                std::cout << "reached pad, exiting string table" << endl;
                break;
            }
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

bios::Table getBIOSAttributeEnumerationTable(bios::BIOSTable& BIOSStringTable,
                                             const char* path)
{
    AttrValuesMap attributeMap = getBIOSValues(path);
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
                  << " from string table for the attribute" << endl;
        uint8_t typeOfAttr =
            ((std::get<0>(value)) ? PLDM_BIOS_ENUMERATION_READ_ONLY
                                  : PLDM_BIOS_ENUMERATION);
        PossibleValues possiVals = std::get<1>(value);
        std::cout << "possible values for the attribute: " << endl;
        for (auto& e : possiVals)
        {
            std::cout << e << " ";
        }
        std::cout << endl;
        DefaultValues defVals = std::get<2>(value);
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
        auto defValsByHdl = findDefaultValHandle(possiVals, defVals);
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
        attrPtr->attr_type = typeOfAttr;
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

bios::Table
    getBIOSAttributeValueEnumerationTable(bios::BIOSTable& BIOSAttributeTable,
                                          bios::BIOSTable& BIOSStringTable)
{
    bios::Table attributeValueTable;
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    size_t respLen = response.size();
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    encode_get_bios_table_resp(0, PLDM_SUCCESS, 0, PLDM_START_AND_END, nullptr,
                               respLen, responsePtr);
    BIOSAttributeTable.load(response);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    struct pldm_get_bios_table_resp* resp =
        reinterpret_cast<struct pldm_get_bios_table_resp*>(
            responsePtr->payload);

    uint8_t* tableData = reinterpret_cast<uint8_t*>(resp->table_data);
    struct pldm_bios_attr_table_entry* attrPtr =
        reinterpret_cast<struct pldm_bios_attr_table_entry*>(tableData);

    uint16_t attrHdl;
    uint16_t stringHdl;
    uint8_t attrType;
    uint8_t numCurrVals;
    std::cout << "inside getBIOSAttributeValueEnumerationTable" << endl;
    uint16_t* test;
    size_t attrTableLen =
        response.size() - sizeof(pldm_msg_hdr) -
        (sizeof(resp->completion_code) + sizeof(resp->next_transfer_handle) +
         sizeof(resp->transfer_flag));
    std::cout << "response table size= " << attrTableLen << endl;
    uint32_t traversed = 0;

    while (1)
    {
        test = reinterpret_cast<uint16_t*>(tableData);
        std::cout << "inside while loop tableData= " << test
                  << " attrPtr = " << attrPtr << endl;
        std::cout << "traversed= " << traversed << endl;
        attrHdl = attrPtr->attr_handle;
        attrType = attrPtr->attr_type;
        stringHdl = attrPtr->string_handle;
        std::cout << "fetched attribute handle " << attrHdl << endl;
        std::cout << "fetched string handle " << stringHdl << endl;
        tableData += sizeof(attrHdl) + sizeof(attrType) + sizeof(stringHdl);
        traversed += sizeof(attrHdl) + sizeof(attrType) + sizeof(stringHdl);
        std::cout << "after adding attr handle, type and name hdl, traversed= "
                  << traversed << endl;
        test = reinterpret_cast<uint16_t*>(tableData);
        std::cout << "after pointing to metaData, tableData= " << test << endl;
        uint8_t numPossiVals = *tableData;
        uint32_t j = numPossiVals;
        std::cout << "number of possi vals " << j << endl;
        tableData++; // pass number of possible values
        traversed += sizeof(numPossiVals);
        std::cout << "after adding num of possible vals, traversed= "
                  << traversed << endl;
        test = reinterpret_cast<uint16_t*>(tableData);
        std::cout << "after numPossiVals, tableData= " << test << endl;
        uint16_t* temp;
        temp = reinterpret_cast<uint16_t*>(tableData);
        std::cout << "temp= " << temp << endl;
        PossibleValuesByHandle possiValsByHdl;
        uint32_t i = 0;
        uint16_t val;
        while (i < (uint32_t)numPossiVals)
        {
            val = *temp;
            std::cout << "possible value handle " << val << endl;
            possiValsByHdl.push_back(val);
            std::cout << "after possiValsByHdl.push_back i= " << i << endl;
            temp++;
            std::cout << "after " << i << "th val, temp= " << temp << endl;
            i++;
        }
        for (auto& e : possiValsByHdl)
        {
            std::cout << "possi val handle " << e << endl;
        }
        traversed += numPossiVals * sizeof(stringHdl);
        std::cout << "after adding all possible vals, traversed= " << traversed
                  << endl;
        tableData = reinterpret_cast<uint8_t*>(temp);
        uint8_t numDefVals = *tableData;
        j = numDefVals;
        std::cout << "number of def vals " << j << endl;
        tableData++;             // pass number of def vals
        tableData += numDefVals; // pass all the def val indices
        traversed += sizeof(numDefVals) + numDefVals;
        std::cout
            << "after adding num of def vals and all def vals size, traversed= "
            << traversed << endl;

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
            findStrIndices(possiValsByHdl, currVals, BIOSStringTable);
        std::cout << "after findStrIndices currValStrIndices.size()"
                  << currValStrIndices.size() << endl;
        for (auto& k : currValStrIndices)
        {
            uint16_t p = k;
            std::cout << "curr val index" << p << endl;
        }

        Row entryToAttrValTable(sizeof(uint16_t) + sizeof(uint8_t) +
                                    sizeof(uint8_t) +
                                    numCurrVals * sizeof(uint8_t),
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
        std::cout << "before if block traversed= " << traversed << endl;

        if (traversed > attrTableLen)
        {
            log<level::ERR>("Error in traversing the attribute table");
            break;
        }
        else if ((attrTableLen - traversed) < 8)
        {
            std::cout << "reached pad, exiting attribute table" << endl;
            break;
        }

        attrPtr =
            reinterpret_cast<struct pldm_bios_attr_table_entry*>(tableData);
        std::cout << "at the end of loop traversed= " << traversed << endl;

    } // end while

    std::cout << "returning from getBIOSAttributeValueEnumerationTable" << endl;

    return attributeValueTable;
} // end getBIOSAttributeValueEnumerationTable

uint8_t calculatePad(uint32_t tableLen)
{
    uint8_t pad;

    pad = ((tableLen % 4) ? (4 - tableLen % 4) : 0);

    uint16_t p = pad;
    std::cout << "calculated pad " << p << " for table length " << endl;
    return pad;
} // end calculatePad

} // namespace responder
} // namespace pldm
