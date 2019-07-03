#include "bios.hpp"

#include "libpldmresponder/utils.hpp"
#include "registration.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <array>
#include <boost/crc.hpp>
#include <chrono>
#include <ctime>
#include <iostream>
#include <numeric>
#include <phosphor-logging/log.hpp>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace pldm
{

using namespace phosphor::logging;
using EpochTimeUS = uint64_t;
using Row = std::vector<uint8_t>;

constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";

namespace responder
{

namespace bios
{

void registerHandlers()
{
    registerHandler(PLDM_BIOS, PLDM_GET_DATE_TIME, std::move(getDateTime));
    registerHandler(PLDM_BIOS, PLDM_GET_BIOS_TABLE, std::move(getBIOSTable));
}

} // namespace bios

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

Response getDateTime(const pldm_msg* request, size_t payloadLength)
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

        encode_get_date_time_resp(request->hdr.instance_id, PLDM_ERROR, seconds,
                                  minutes, hours, day, month, year,
                                  responsePtr);
        return response;
    }

    uint64_t timeUsec = std::get<EpochTimeUS>(value);

    uint64_t timeSec = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::microseconds(timeUsec))
                           .count();

    utils::epochToBCDTime(timeSec, seconds, minutes, hours, day, month, year);

    encode_get_date_time_resp(request->hdr.instance_id, PLDM_SUCCESS, seconds,
                              minutes, hours, day, month, year, responsePtr);
    return response;
}

/** @brief Construct the BIOS string table
 *
 *  @param[in] BIOSStringTable - in memory copy of the string table
 *  @param[in] transferHandle - transfer handle to identify part of transfer
 *  @param[in] transferOpFlag - flag to indicate which part of data being
 * transferred
 *  @param[in] instanceID - instance ID to identify the command
 *  @param[in] path - path where the BIOS tables will be persisted
 */
Response getBIOSStringTable(BIOSTable& BIOSStringTable, uint32_t transferHandle,
                            uint8_t transferOpFlag, uint8_t instanceID,
                            const char* biosJsonDir)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (BIOSStringTable.isEmpty())
    { // no persisted table, constructing fresh table and file
        auto biosStrings = bios_parser::getStrings(biosJsonDir);
        std::sort(biosStrings.begin(), biosStrings.end());
        biosStrings.erase(std::unique(biosStrings.begin(), biosStrings.end()),
                          biosStrings.end());
        uint32_t allStringsLen = std::accumulate(
            biosStrings.begin(), biosStrings.end(), 0,
            [](int sum, const std::string& elem) { return sum + elem.size(); });
        uint32_t checkSum;
        uint32_t sizeWithoutPad =
            allStringsLen +
            (biosStrings.size() * (sizeof(pldm_bios_string_table_entry) - 1));
        uint8_t padSize = calculatePad(sizeWithoutPad);
        uint32_t stringTableSize;
        if (biosStrings.size())
        {
            stringTableSize = sizeWithoutPad + padSize + sizeof(checkSum);
        }
        else
        {
            stringTableSize = 0;
        }

        Table stringTable(
            stringTableSize,
            0); // initializing to 0 so that pad will be automatically added
        uint8_t* tablePtr = reinterpret_cast<uint8_t*>(stringTable.data());

        for (const auto& elem : biosStrings)
        {
            struct pldm_bios_string_table_entry* stringPtr =
                reinterpret_cast<struct pldm_bios_string_table_entry*>(
                    tablePtr);

            stringPtr->string_handle = []() -> uint16_t {
                static uint16_t hdl = 0;
                return hdl++;
            }();
            stringPtr->string_length = elem.length();
            memcpy(stringPtr->name, elem.c_str(), elem.length());
            tablePtr += 2 * sizeof(uint16_t);
            tablePtr += elem.length();
        }
        tablePtr += padSize;

        if (stringTableSize)
        {
            /* compute checksum*/
            boost::crc_32_type result;
            result.process_bytes(stringTable.data(), stringTableSize);
            checkSum = result.checksum();
            uint32_t* cSumPtr = reinterpret_cast<uint32_t*>(tablePtr);
            *cSumPtr = checkSum;
            BIOSStringTable.store(stringTable);
        }

        response.resize(sizeof(pldm_msg_hdr) +
                            PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                            stringTableSize,
                        0);
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        size_t respPayloadLength = response.size();
        uint32_t nxtTransferHandle = 0;
        uint8_t transferFlag = PLDM_START_AND_END;
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, stringTable.data(),
                                   respPayloadLength, responsePtr);
    }

    else
    { /* only persisted table present, constructing response*/
        size_t respPayloadLength = response.size();
        uint32_t nxtTransferHandle = 0;
        uint8_t transferFlag = PLDM_START_AND_END;
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr); // filling up the header here
        BIOSStringTable.load(response);
    }

    return response;
}

Response getBIOSTable(const pldm_msg* request, size_t payloadLength)
{
    const char* biosJsonDir = BIOS_JSONS_DIR;
    const char* biosTablePath = BIOS_TABLES_DIR;

    auto response =
        buildBIOSTables(request, payloadLength, biosJsonDir, biosTablePath);

    return response;
}

Response buildBIOSTables(const pldm_msg* request, size_t payloadLength,
                         const char* biosJsonDir, const char* biosTablePath)
{
    constexpr auto biosStringTableFilePath = "/stringTableFile";
    constexpr auto biosAttributeTableFilePath = "/attributeTableFile";
    constexpr auto biosAttributeValueTablePath = "/attributeValueTableFile";

    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    uint32_t transferHandle{};
    uint8_t transferOpFlag{};
    uint8_t tableType{};

    auto rc = decode_get_bios_table_req(request, payloadLength, &transferHandle,
                                        &transferOpFlag, &tableType);

    if (rc == PLDM_SUCCESS)
    {
        BIOSTable BIOSStringTable(
            (std::string(biosTablePath) + std::string(biosStringTableFilePath))
                .c_str());
        BIOSTable BIOSAttributeTable((std::string(biosTablePath) +
                                      std::string(biosAttributeTableFilePath))
                                         .c_str());
        BIOSTable BIOSAttributeValueTable(
            (std::string(biosTablePath) +
             std::string(biosAttributeValueTablePath))
                .c_str());

        switch (tableType)
        {
            case PLDM_BIOS_STRING_TABLE:
                response = getBIOSStringTable(
                    BIOSStringTable, transferHandle, transferOpFlag,
                    request->hdr.instance_id, biosJsonDir);

                break;
            case PLDM_BIOS_ATTR_TABLE:

                if (BIOSStringTable.isEmpty())
                {
                    rc = PLDM_BIOS_TABLE_UNAVAILABLE;
                }
                else
                {
                    /*response = getBIOSAttributeTable(
                        BIOSAttributeTable, BIOSStringTable, transferHandle,
                        transferOpFlag, instanceID, path);*/
                }
                break;
            case PLDM_BIOS_ATTR_VAL_TABLE:
                if (BIOSAttributeTable.isEmpty())
                {
                    rc = PLDM_BIOS_TABLE_UNAVAILABLE;
                }
                else
                {
                    /*response = getBIOSAttributeValueTable(
                        BIOSAttributeValueTable, BIOSAttributeTable,
                        BIOSStringTable, transferHandle, transferOpFlag,
                        instanceID, path);*/
                }
                break;
            default:
                rc = PLDM_INVALID_BIOS_TABLE_TYPE;
                break;
        }
    } // end if

    if (rc != PLDM_SUCCESS)
    {
        uint32_t nxtTransferHandle{};
        uint8_t transferFlag{};
        size_t respPayloadLength{};
        encode_get_bios_table_resp(request->hdr.instance_id, rc,
                                   nxtTransferHandle, transferFlag, nullptr,
                                   respPayloadLength, responsePtr);
    }

    return response;
} // end buildBIOSTables

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
    uint32_t dummytransferHandle{};
    uint8_t dummytransferOpFlag{};
    Response dummyResponse =
        getBIOSStringTable(BIOSStringTable, dummytransferHandle,
                           dummytransferOpFlag, instanceID, path);

    if (BIOSAttributeTable.isEmpty())
    { // no persisted table, constructing fresh table and data
        bios::Table attributeTable =
            getBIOSAttributeEnumerationTable(BIOSStringTable, path);

        // calculate pad
        uint8_t padSize = calculatePad(attributeTable.size());
        std::vector<uint8_t> pad(padSize, 0);
        if (padSize)
        {
            attributeTable.insert(attributeTable.end(), pad.begin(), pad.end());
        }

        // compute checksum
        boost::crc_32_type result;
        result.process_bytes(attributeTable.data(), attributeTable.size());
        uint32_t checkSum = result.checksum();
        uint32_t size = attributeTable.size();
        attributeTable.resize(size + sizeof(checkSum));
        uint8_t* ck = reinterpret_cast<uint8_t*>(&checkSum);
        std::copy(ck, ck + sizeof(checkSum), attributeTable.data() + size);

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
        bios::Table attributeValueTable = getBIOSAttributeValueEnumerationTable(
            BIOSAttributeTable, BIOSStringTable);
        // calculate pad
        uint8_t padSize = calculatePad(attributeValueTable.size());
        std::vector<uint8_t> pad(padSize, 0);
        if (padSize)
        {
            attributeValueTable.insert(attributeValueTable.end(), pad.begin(),
                                       pad.end());
        }

        // compute checksum
        boost::crc_32_type result;
        result.process_bytes(attributeValueTable.data(),
                             attributeValueTable.size());
        uint32_t checkSum = result.checksum();
        uint32_t size = attributeValueTable.size();
        attributeValueTable.resize(size + sizeof(checkSum));
        uint8_t* ck = reinterpret_cast<uint8_t*>(&checkSum);
        std::copy(ck, ck + sizeof(checkSum), attributeValueTable.data() + size);

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

/*
Strings getBIOSStrings(const char* dirPath) // to be removed after Tom's patch
{
    Strings biosStrings = {"This",      "On",        "Off",     "HMCManaged",
                           "Enabled",   "Disabled",  "IPLSide", "USBEmulation",
                           "Persisted", "DumpPolicy"};
    return biosStrings;
}*/
/*
AttrValuesMap
    getBIOSValues(const char* dirPath) // to be removed after Tom's patch
{
    AttrValuesMap attrMap;
    std::tuple<ReadOnly, PossibleValues, DefaultValues> tp;

    PossibleValues pos = {"Persisted", "Enabled", "Disabled"};
    DefaultValues def = {"Disabled"};
    tp = std::make_tuple(false, pos, def);
    attrMap["DumpPolicy"] = tp;

    return attrMap;
}*/
/*
CurrentValues
    getAttrValue(const std::string& attrName) // to be removed after Tom's patch
{
    // considering attribute name("DumpPolicy");
    CurrentValues currVals;
    currVals.push_back("Enabled");
    return currVals;
} // end getAttrValue
*/
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
        handle temp = ptr->string_handle;
        uint16_t len = ptr->string_length;
        if (memcmp(name.c_str(), ptr->name, len) == 0)
        {
            hdl = temp;
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
            break;
        }

        ptr = reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    }
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
        handle temp = ptr->string_handle;
        uint16_t len = ptr->string_length;
        if (temp == stringHandle)
        {
            name.resize(len);
            memcpy(name.data(), ptr->name, len);
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
                break;
            }
            i++;
        } // end inner for
    }     // end for
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
    for (auto& defs : defVals)
    {
        i = 0;
        for (auto& possi : possiVals)
        {
            if (possi.compare(defs) == 0)
            {
                defHdls.push_back(i);
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
    AttrValuesMap attributeMap = getValues(path);
    // AttrValuesMapByHandle attrMapByHdl;
    PossibleValuesByHandle possibleValByHdl;
    DefaultValuesByHandle defaultValByHdl;
    bios::Table attributeTable;

    for (auto& [key, value] : attributeMap)
    {
        handle attrNameHdl = findStringHandle(key, BIOSStringTable);
        uint8_t typeOfAttr =
            ((std::get<0>(value)) ? PLDM_BIOS_ENUMERATION_READ_ONLY
                                  : PLDM_BIOS_ENUMERATION);
        PossibleValues possiVals = std::get<1>(value);
        DefaultValues defVals = std::get<2>(value);

        auto possiValsByHdl = findStringHandle(possiVals, BIOSStringTable);
        auto defValsByHdl = findDefaultValHandle(possiVals, defVals);

        Row entryToAttrTable(
            sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t) +
                sizeof(uint8_t) + possiValsByHdl.size() * sizeof(uint16_t) +
                sizeof(uint8_t) + defValsByHdl.size() * sizeof(uint8_t),
            0);
        struct pldm_bios_attr_table_entry* attrPtr =
            reinterpret_cast<struct pldm_bios_attr_table_entry*>(
                entryToAttrTable.data());
        attrPtr->attr_handle = nextAttrHandle();
        attrPtr->attr_type = typeOfAttr;
        attrPtr->string_handle = attrNameHdl;
        uint8_t* metaData = reinterpret_cast<uint8_t*>(attrPtr->metadata);
        *metaData = possiValsByHdl.size();
        metaData++;
        uint16_t* metaDataVal = reinterpret_cast<uint16_t*>(metaData);
        for (auto& elem : possiValsByHdl)
        {
            *metaDataVal = elem;
            metaDataVal++;
        } // end for
        metaData = reinterpret_cast<uint8_t*>(metaDataVal);
        *metaData = defValsByHdl.size();
        metaData++;
        for (auto& elem : defValsByHdl)
        {
            *metaData = elem;
            metaData++;
        } // end for

        attributeTable.insert(attributeTable.end(), entryToAttrTable.begin(),
                              entryToAttrTable.end());
    } // end outer for

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
    size_t attrTableLen =
        response.size() - sizeof(pldm_msg_hdr) -
        (sizeof(resp->completion_code) + sizeof(resp->next_transfer_handle) +
         sizeof(resp->transfer_flag));
    uint32_t traversed = 0;

    while (1)
    {
        attrHdl = attrPtr->attr_handle;
        attrType = attrPtr->attr_type;
        stringHdl = attrPtr->string_handle;
        tableData += sizeof(attrHdl) + sizeof(attrType) + sizeof(stringHdl);
        traversed += sizeof(attrHdl) + sizeof(attrType) + sizeof(stringHdl);
        uint8_t numPossiVals = *tableData;
        tableData++; // pass number of possible values
        traversed += sizeof(numPossiVals);
        uint16_t* temp;
        temp = reinterpret_cast<uint16_t*>(tableData);
        PossibleValuesByHandle possiValsByHdl;
        uint32_t i = 0;
        uint16_t val;
        while (i < (uint32_t)numPossiVals)
        {
            val = *temp;
            possiValsByHdl.push_back(val);
            temp++;
            i++;
        }
        traversed += numPossiVals * sizeof(stringHdl);
        tableData = reinterpret_cast<uint8_t*>(temp);
        uint8_t numDefVals = *tableData;
        tableData++;             // pass number of def vals
        tableData += numDefVals; // pass all the def val indices
        traversed += sizeof(numDefVals) + numDefVals;

        std::string attrName = findStringName(stringHdl, BIOSStringTable);
        CurrentValues currVals = getAttrValue(attrName);
        numCurrVals = currVals.size();
        auto currValStrIndices =
            findStrIndices(possiValsByHdl, currVals, BIOSStringTable);

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

        if (traversed > attrTableLen)
        {
            log<level::ERR>("Error in traversing the attribute table");
            break;
        }
        else if ((attrTableLen - traversed) < 8)
        {
            break;
        }

        attrPtr =
            reinterpret_cast<struct pldm_bios_attr_table_entry*>(tableData);

    } // end while

    return attributeValueTable;
} // end getBIOSAttributeValueEnumerationTable

uint8_t calculatePad(uint32_t tableLen)
{
    uint8_t pad;

    pad = ((tableLen % 4) ? (4 - tableLen % 4) : 0);

    return pad;
} // end calculatePad

} // namespace responder
} // namespace pldm
