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
using BIOSTableRow = std::vector<uint8_t>;
using DefaultValuesByHandle = std::vector<handle>;

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
 *  @param[in] BIOSStringTable - copy of the string table
 *  @param[in] transferHandle - transfer handle to identify part of transfer
 *  @param[in] transferOpFlag - flag to indicate which part of data being
 * transferred
 *  @param[in] instanceID - instance ID to identify the command
 *  @param[in] biosJsonDir - path where the BIOS tables will be persisted
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
        size_t allStringsLen =
            std::accumulate(biosStrings.begin(), biosStrings.end(), 0,
                            [](size_t sum, const std::string& elem) {
                                return sum + elem.size();
                            });
        size_t sizeWithoutPad =
            allStringsLen +
            (biosStrings.size() * (sizeof(pldm_bios_string_table_entry) - 1));
        uint8_t padSize = utils::getNumPadBytes(sizeWithoutPad);
        uint32_t stringTableSize{};
        uint32_t checkSum;
        if (biosStrings.size())
        {
            stringTableSize = sizeWithoutPad + padSize + sizeof(checkSum);
        }
        auto nextStringHandle = []() -> uint16_t {
            static uint16_t hdl = 0;
            return hdl++;
        };
        Table stringTable(
            stringTableSize,
            0); // initializing to 0 so that pad will be automatically added
        auto tablePtr = reinterpret_cast<uint8_t*>(stringTable.data());
        for (const auto& elem : biosStrings)
        {
            auto stringPtr =
                reinterpret_cast<struct pldm_bios_string_table_entry*>(
                    tablePtr);

            stringPtr->string_handle = nextStringHandle();
            stringPtr->string_length = elem.length();
            memcpy(stringPtr->name, elem.c_str(), elem.length());
            tablePtr += sizeof(stringPtr->string_handle) +
                        sizeof(stringPtr->string_length);
            tablePtr += elem.length();
        }
        tablePtr += padSize;

        if (stringTableSize)
        {
            /* compute checksum*/
            boost::crc_32_type result;
            result.process_bytes(stringTable.data(), stringTableSize);
            checkSum = result.checksum();
            std::copy_n(reinterpret_cast<uint8_t*>(&checkSum), sizeof(checkSum),
                        stringTable.data() + sizeWithoutPad + padSize);
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
} // end getBIOSStringTable

/** @brief Find the string handle from the BIOS string table given the name
 *
 *  @param[in] name - name of the string
 *  @param[in] BIOSStringTable - copy of the string table
 *  @return - uint16_t - handle of the string
 */
handle findStringHandle(const std::string& name,
                        const BIOSTable& BIOSStringTable)
{
    handle hdl{};
    Response response;
    BIOSStringTable.load(response);

    auto tableData = reinterpret_cast<uint8_t*>(response.data());
    auto tableEntry =
        reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    uint32_t tableLen = response.size();
    size_t traversed = 0;
    while (1)
    {
        handle temp = tableEntry->string_handle;
        uint16_t len = tableEntry->string_length;
        if (memcmp(name.c_str(), tableEntry->name, len) == 0)
        {
            hdl = temp;
            break;
        }
        tableData += sizeof(handle) + sizeof(len) + len;
        traversed += sizeof(handle) + sizeof(len) + len;

        if ((tableLen - traversed) < 8)
        {
            break;
        }

        tableEntry =
            reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    }
    return hdl;
} // end findStringHandle

/** @brief Find the string name from the BIOS string table for a string handle
 *
 *  @param[in] stringHandle - string handle
 *  @param[in] BIOSStringTable - copy of the string table
 *
 *  @return - std::string - name of the corresponding string
 */
std::string findStringName(handle stringHandle,
                           const BIOSTable& BIOSStringTable)
{
    std::string name;
    Response response;
    BIOSStringTable.load(response);

    auto tableData = reinterpret_cast<uint8_t*>(response.data());
    auto tableEntry =
        reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    uint32_t tableLen = response.size();
    size_t traversed = 0;
    while (1)
    {
        handle tempHdl = tableEntry->string_handle;
        uint16_t len = tableEntry->string_length;
        if (tempHdl == stringHandle)
        {
            name.resize(len);
            memcpy(name.data(), tableEntry->name, len);
            break;
        } // end if
        tableData += (sizeof(struct pldm_bios_string_table_entry) - 1) + len;
        traversed += (sizeof(struct pldm_bios_string_table_entry) - 1) + len;

        if ((tableLen - traversed) < 8)
        {
            break;
        }

        tableEntry =
            reinterpret_cast<struct pldm_bios_string_table_entry*>(tableData);
    } // end while
    return name;
} // end findStringName

/** @brief Find the indices  into the array of the possible values of string
 *  handles for the current values.This is used in attribute value table
 *
 *  @param[in] possiVals - vector of string handles comprising all the possible
 *                         values for an attribute
 *  @param[in] currVals - vector of strings comprising all current values
 *                        for an attribute
 *  @param[in] BIOSStringTable - copy of the string table
 *
 *  @return - std::vector<uint8_t> - indices into the array of the possible
 *                                   values of string handles
 */
std::vector<uint8_t> findStrIndices(PossibleValuesByHandle possiVals,
                                    CurrentValues currVals,
                                    const BIOSTable& BIOSStringTable)
{
    std::vector<uint8_t> stringIndices;

    for (const auto& currVal : currVals)
    {
        auto curHdl = findStringHandle(currVal, BIOSStringTable);
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

/** @brief Find the indices into the array of the possible values of string
 *  handles for the default values. This is used in attribute table
 *
 *  @param[in] possiVals - vector of strings comprising all the possible values
 *                         for an attribute
 *  @param[in] defVals - vector of strings comprising all the default values
 *                       for an attribute
 *  @return - std::vector<uint8_t> - indices into the array of the possible
 *                                   values of string
 */
std::vector<uint8_t> findDefaultValHandle(const PossibleValues& possiVals,
                                          const DefaultValues& defVals)
{
    std::vector<uint8_t> defHdls;
    for (const auto& defs : defVals)
    {
        auto index = std::lower_bound(possiVals.begin(), possiVals.end(), defs);
        if (index != possiVals.end())
        {
            defHdls.push_back(index - possiVals.begin());
        }
    }

    return defHdls;
} // end findDefaultValHandle

namespace bios_table_enum
{

/** @brief Construct the attibute table for BIOS type Enumeration and
 *         Enumeration ReadOnly
 *  @param[in] BIOSStringTable - copy of the string table
 *  @param[in] path - place where the table will be persisted for future use
 *
 *  @return - Table - the attribute eenumeration table
 */
Table getBIOSAttributeEnumerationTable(const BIOSTable& BIOSStringTable,
                                       const char* biosJsonDir)
{
    setupValueLookup(biosJsonDir);
    auto attributeMap = getValues();
    Table attributeTable;
    auto nextAttributeHandle = []() -> uint16_t {
        static uint16_t hdl = 0;
        return hdl++;
    };

    for (const auto& [key, value] : attributeMap)
    {
        uint8_t typeOfAttr = (std::get<0>(value))
                                 ? PLDM_BIOS_ENUMERATION_READ_ONLY
                                 : PLDM_BIOS_ENUMERATION;
        PossibleValues possiVals = std::get<1>(value);
        DefaultValues defVals = std::get<2>(value);
        std::sort(possiVals.begin(), possiVals.end());
        std::sort(defVals.begin(), defVals.end());

        std::vector<handle> possiValsByHdl;
        for (const auto& elem : possiVals)
        {
            possiValsByHdl.push_back(findStringHandle(elem, BIOSStringTable));
        }
        auto defValsByHdl = findDefaultValHandle(possiVals, defVals);

        BIOSTableRow entryToAttrTable(
            (sizeof(struct pldm_bios_attr_table_entry) - 1) + sizeof(uint8_t) +
                possiValsByHdl.size() * sizeof(uint16_t) + sizeof(uint8_t) +
                defValsByHdl.size() * sizeof(uint8_t),
            0);
        BIOSTableRow::iterator it = entryToAttrTable.begin();
        auto attrPtr = reinterpret_cast<struct pldm_bios_attr_table_entry*>(
            entryToAttrTable.data());
        attrPtr->attr_handle = nextAttributeHandle();
        attrPtr->attr_type = typeOfAttr;
        attrPtr->string_handle = findStringHandle(key, BIOSStringTable);
        std::advance(it, (sizeof(struct pldm_bios_attr_table_entry) - 1));
        uint8_t tempNumPosVal = possiValsByHdl.size();
        std::copy_n(reinterpret_cast<uint8_t*>(&tempNumPosVal),
                    sizeof(tempNumPosVal), it);
        std::advance(it, sizeof(tempNumPosVal));
        std::copy_n(reinterpret_cast<uint8_t*>(possiValsByHdl.data()),
                    2 * possiValsByHdl.size(), it);
        std::advance(
            it, 2 * possiValsByHdl.size()); // possible val handle is uint16_t
        uint8_t tempNumDefVals = defValsByHdl.size();
        std::copy_n(reinterpret_cast<uint8_t*>(&tempNumDefVals),
                    sizeof(tempNumDefVals), it);
        std::advance(it, sizeof(tempNumDefVals));
        std::copy(defValsByHdl.begin(), defValsByHdl.end(), it);
        std::advance(it, defValsByHdl.size());

        attributeTable.insert(attributeTable.end(), entryToAttrTable.begin(),
                              entryToAttrTable.end());
    } // end for

    return attributeTable;
}

/** @brief Construct the attibute value table for BIOS type Enumeration and
 *  Enumeration ReadOnly
 *
 *  @param[in] BIOSAttributeTable - copy of the attribute table
 *  @param[in] BIOSStringTable - copy of the string table
 *
 *  @return - Table - the attribute value table
 */
Table getBIOSAttributeValueEnumerationTable(const BIOSTable& BIOSAttributeTable,
                                            const BIOSTable& BIOSStringTable)
{
    Table attributeValueTable;
    Response response;
    BIOSAttributeTable.load(response);

    auto tableData = reinterpret_cast<uint8_t*>(response.data());
    auto attrPtr =
        reinterpret_cast<struct pldm_bios_attr_table_entry*>(tableData);

    size_t attrTableLen = response.size();
    size_t traversed = 0;

    while (1)
    {
        uint16_t attrHdl = attrPtr->attr_handle;
        uint8_t attrType = attrPtr->attr_type;
        uint16_t stringHdl = attrPtr->string_handle;
        tableData += (sizeof(struct pldm_bios_attr_table_entry) - 1);
        traversed += (sizeof(struct pldm_bios_attr_table_entry) - 1);
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

        auto attrName = findStringName(stringHdl, BIOSStringTable);
        auto currVals = getAttrValue(attrName);
        std::sort(currVals.begin(), currVals.end());
        uint8_t numCurrVals = currVals.size();
        auto currValStrIndices =
            findStrIndices(possiValsByHdl, currVals, BIOSStringTable);

        BIOSTableRow entryToAttrValTable(sizeof(uint16_t) + sizeof(uint8_t) +
                                             sizeof(uint8_t) +
                                             numCurrVals * sizeof(uint8_t),
                                         0);
        BIOSTableRow::iterator it = entryToAttrValTable.begin();
        auto attrValPtr =
            reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(
                entryToAttrValTable.data());
        attrValPtr->attr_handle = attrHdl;
        attrValPtr->attr_type = attrType;
        std::advance(it, (sizeof(pldm_bios_attr_val_table_entry) - 1));
        std::copy_n(reinterpret_cast<uint8_t*>(&numCurrVals),
                    sizeof(numCurrVals), it);
        std::advance(it, sizeof(numCurrVals));
        std::copy(currValStrIndices.begin(), currValStrIndices.end(), it);
        std::advance(it, currValStrIndices.size());
        attributeValueTable.insert(attributeValueTable.end(),
                                   entryToAttrValTable.begin(),
                                   entryToAttrValTable.end());

        if ((attrTableLen - traversed) < 8)
        {
            break;
        }

        attrPtr =
            reinterpret_cast<struct pldm_bios_attr_table_entry*>(tableData);

    } // end while

    return attributeValueTable;
} // end getBIOSAttributeValueEnumerationTable

} // end namespace bios_table_enum

/** @brief Construct the BIOS attribute table
 *
 *  @param[in] BIOSAttributeTable - copy of the attribute table
 *  @param[in] BIOSStringTable - copy of the string table
 *  @param[in] transferHandle - transfer handle to identify part of transfer
 *  @param[in] transferOpFlag - flag to indicate which part of data being
 * transferred
 *  @param[in] instanceID - instance ID to identify the command
 *  @param[in] biosJsonDir - path where the BIOS tables will be persisted
 */
Response getBIOSAttributeTable(BIOSTable& BIOSAttributeTable,
                               const BIOSTable& BIOSStringTable,
                               uint32_t transferHandle, uint8_t transferOpFlag,
                               uint8_t instanceID, const char* biosJsonDir)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;
    size_t respPayloadLength{};

    if (BIOSAttributeTable.isEmpty())
    { /* no persisted table, constructing fresh table and response*/
        Table attributeTable =
            bios_table_enum::getBIOSAttributeEnumerationTable(BIOSStringTable,
                                                              biosJsonDir);

        /* calculate pad */
        uint8_t padSize = utils::getNumPadBytes(attributeTable.size());
        std::vector<uint8_t> pad(padSize, 0);
        if (padSize)
        {
            attributeTable.insert(attributeTable.end(), pad.begin(), pad.end());
        }

        if (attributeTable.size())
        {
            /* compute checksum */
            boost::crc_32_type result;
            result.process_bytes(attributeTable.data(), attributeTable.size());
            uint32_t checkSum = result.checksum();
            size_t size = attributeTable.size();
            attributeTable.resize(size + sizeof(checkSum));
            std::copy_n(reinterpret_cast<uint8_t*>(&checkSum), sizeof(checkSum),
                        attributeTable.data() + size);
            BIOSAttributeTable.store(attributeTable);
        }
        response.resize(sizeof(pldm_msg_hdr) +
                        PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                        attributeTable.size());
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, attributeTable.data(),
                                   respPayloadLength, responsePtr);
    } // end if
    else
    { // only persisted table present, constructing response
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr); // filling up the header here
        BIOSAttributeTable.load(response);
    } // end else

    return response;
} // end getBIOSAttributeTable

/** @brief Construct the BIOS attribute value table
 *
 *  @param[in] BIOSAttributeValueTable - copy of the attribute value table
 *  @param[in] BIOSAttributeTable - copy of the attribute table
 *  @param[in] BIOSStringTable - copy of the string table
 *  @param[in] transferHandle - transfer handle to identify part of transfer
 *  @param[in] transferOpFlag - flag to indicate which part of data being
 * transferred
 *  @param[in] instanceID - instance ID to identify the command
 *  @param[in] biosJsonDir -  path where the BIOS tables will be persisted
 */
Response getBIOSAttributeValueTable(BIOSTable& BIOSAttributeValueTable,
                                    const BIOSTable& BIOSAttributeTable,
                                    const BIOSTable& BIOSStringTable,
                                    uint32_t& transferHandle,
                                    uint8_t& transferOpFlag, uint8_t instanceID,
                                    const char* biosJsonDir)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;
    size_t respPayloadLength{};

    if (BIOSAttributeValueTable.isEmpty())
    { // no persisted table, constructing fresh table and data
        Table attributeValueTable =
            bios_table_enum::getBIOSAttributeValueEnumerationTable(
                BIOSAttributeTable, BIOSStringTable);
        // calculate pad
        uint8_t padSize = utils::getNumPadBytes(attributeValueTable.size());
        std::vector<uint8_t> pad(padSize, 0);
        if (padSize)
        {
            attributeValueTable.insert(attributeValueTable.end(), pad.begin(),
                                       pad.end());
        }
        if (attributeValueTable.size())
        {
            /* compute checksum */
            boost::crc_32_type result;
            result.process_bytes(attributeValueTable.data(),
                                 attributeValueTable.size());
            uint32_t checkSum = result.checksum();
            size_t size = attributeValueTable.size();
            attributeValueTable.resize(size + sizeof(checkSum));
            std::copy_n(reinterpret_cast<uint8_t*>(&checkSum), sizeof(checkSum),
                        attributeValueTable.data() + size);
            BIOSAttributeValueTable.store(attributeValueTable);
        }

        response.resize(sizeof(pldm_msg_hdr) +
                        PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                        attributeValueTable.size());
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, attributeValueTable.data(),
                                   respPayloadLength, responsePtr);
    }
    else
    { // only persisted table present, constructing response
        respPayloadLength = response.size();
        encode_get_bios_table_resp(instanceID, PLDM_SUCCESS, nxtTransferHandle,
                                   transferFlag, nullptr, respPayloadLength,
                                   responsePtr); // filling up the header here
        BIOSAttributeValueTable.load(response);
    } // end else

    return response;
} // end getBIOSAttributeValueTable

Response getBIOSTable(const pldm_msg* request, size_t payloadLength)
{
    auto response = buildBIOSTables(request, payloadLength, BIOS_JSONS_DIR,
                                    BIOS_TABLES_DIR);

    return response;
}

namespace bios
{

void registerHandlers()
{
    registerHandler(PLDM_BIOS, PLDM_GET_DATE_TIME, std::move(getDateTime));
    registerHandler(PLDM_BIOS, PLDM_GET_BIOS_TABLE, std::move(getBIOSTable));
}

Response buildBIOSTables(const pldm_msg* request, size_t payloadLength,
                         const char* biosJsonDir, const char* biosTablePath)
{
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
            ((std::string(biosTablePath) + "/stringTable")).c_str());
        BIOSTable BIOSAttributeTable(
            ((std::string(biosTablePath) + "/attributeTable")).c_str());
        BIOSTable BIOSAttributeValueTable(
            ((std::string(biosTablePath) + "/attributeValueTable")).c_str());
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
                    response = getBIOSAttributeTable(
                        BIOSAttributeTable, BIOSStringTable, transferHandle,
                        transferOpFlag, request->hdr.instance_id, biosJsonDir);
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
                        request->hdr.instance_id, biosJsonDir);
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
} // namespace bios

} // namespace responder
} // namespace pldm
