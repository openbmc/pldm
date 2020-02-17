#include "bios.hpp"

#include "utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <time.h>

#include <array>
#include <boost/crc.hpp>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace fs = std::filesystem;
using namespace pldm::responder::bios;
using namespace bios_parser;

constexpr auto stringTableFile = "stringTable";
constexpr auto attrTableFile = "attributeTable";
constexpr auto attrValTableFile = "attributeValueTable";

namespace pldm
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using EpochTimeUS = uint64_t;
using BIOSTableRow = std::vector<uint8_t>;
using BIOSJsonName = std::string;

constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";
constexpr auto padChksumMax = 7;

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

    seconds = pldm::utils::decimalToBcd(time->tm_sec);
    minutes = pldm::utils::decimalToBcd(time->tm_min);
    hours = pldm::utils::decimalToBcd(time->tm_hour);
    day = pldm::utils::decimalToBcd(time->tm_mday);
    month = pldm::utils::decimalToBcd(time->tm_mon +
                                      1); // The number of months in the range
                                          // 0 to 11.PLDM expects range 1 to 12
    year = pldm::utils::decimalToBcd(time->tm_year +
                                     1900); // The number of years since 1900
}

std::time_t timeToEpoch(uint8_t seconds, uint8_t minutes, uint8_t hours,
                        uint8_t day, uint8_t month, uint16_t year)
{
    struct std::tm stm;

    stm.tm_year = year - 1900;
    stm.tm_mon = month - 1;
    stm.tm_mday = day;
    stm.tm_hour = hours;
    stm.tm_min = minutes;
    stm.tm_sec = seconds;
    stm.tm_isdst = -1;

    // It will get the time in seconds since
    // Epoch, 1970.1.1 00:00:00 +0000,UTC.
    return timegm(&stm);
}

size_t getTableTotalsize(size_t sizeWithoutPad)
{
    return sizeWithoutPad + pldm_bios_table_pad_checksum_size(sizeWithoutPad);
}

void padAndChecksum(Table& table)
{
    auto sizeWithoutPad = table.size();
    auto padAndChecksumSize = pldm_bios_table_pad_checksum_size(sizeWithoutPad);
    table.resize(table.size() + padAndChecksumSize);

    pldm_bios_table_append_pad_checksum(table.data(), table.size(),
                                        sizeWithoutPad);
}

} // namespace utils

namespace bios
{

Handler::Handler() : biosConfig(BIOS_JSONS_DIR, BIOS_TABLES_DIR)
{
    biosConfig.removeTables();
    biosConfig.buildTables();

    handlers.emplace(PLDM_SET_DATE_TIME,
                     [this](const pldm_msg* request, size_t payloadLength) {
                         return this->setDateTime(request, payloadLength);
                     });
    handlers.emplace(PLDM_GET_DATE_TIME,
                     [this](const pldm_msg* request, size_t payloadLength) {
                         return this->getDateTime(request, payloadLength);
                     });
    handlers.emplace(PLDM_GET_BIOS_TABLE,
                     [this](const pldm_msg* request, size_t payloadLength) {
                         return this->getBIOSTable(request, payloadLength);
                     });
    handlers.emplace(PLDM_GET_BIOS_ATTRIBUTE_CURRENT_VALUE_BY_HANDLE,
                     [this](const pldm_msg* request, size_t payloadLength) {
                         return this->getBIOSAttributeCurrentValueByHandle(
                             request, payloadLength);
                     });
    handlers.emplace(PLDM_SET_BIOS_ATTRIBUTE_CURRENT_VALUE,
                     [this](const pldm_msg* request, size_t payloadLength) {
                         return this->setBIOSAttributeCurrentValue(
                             request, payloadLength);
                     });
}

Response Handler::getDateTime(const pldm_msg* request, size_t /*payloadLength*/)
{
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    constexpr auto timeInterface = "xyz.openbmc_project.Time.EpochTime";
    constexpr auto hostTimePath = "/xyz/openbmc_project/time/host";
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_DATE_TIME_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    EpochTimeUS timeUsec;

    try
    {
        timeUsec = pldm::utils::DBusHandler().getDbusProperty<EpochTimeUS>(
            hostTimePath, "Elapsed", timeInterface);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << "Error getting time, PATH=" << hostTimePath
                  << " TIME INTERACE=" << timeInterface << "\n";

        return CmdHandler::ccOnlyResponse(request, PLDM_ERROR);
    }

    uint64_t timeSec = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::microseconds(timeUsec))
                           .count();

    pldm::responder::utils::epochToBCDTime(timeSec, seconds, minutes, hours,
                                           day, month, year);

    auto rc = encode_get_date_time_resp(request->hdr.instance_id, PLDM_SUCCESS,
                                        seconds, minutes, hours, day, month,
                                        year, responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::setDateTime(const pldm_msg* request, size_t payloadLength)
{
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;
    std::time_t timeSec;

    constexpr auto setTimeInterface = "xyz.openbmc_project.Time.EpochTime";
    constexpr auto setTimePath = "/xyz/openbmc_project/time/host";
    constexpr auto timeSetPro = "Elapsed";

    auto rc = decode_set_date_time_req(request, payloadLength, &seconds,
                                       &minutes, &hours, &day, &month, &year);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }
    timeSec = pldm::responder::utils::timeToEpoch(seconds, minutes, hours, day,
                                                  month, year);
    uint64_t timeUsec = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::seconds(timeSec))
                            .count();
    std::variant<uint64_t> value{timeUsec};
    try
    {
        pldm::utils::DBusHandler().setDbusProperty(setTimePath, timeSetPro,
                                                   setTimeInterface, value);
    }
    catch (std::exception& e)
    {

        std::cerr << "Error Setting time,PATH=" << setTimePath
                  << "TIME INTERFACE=" << setTimeInterface
                  << "ERROR=" << e.what() << "\n";

        return ccOnlyResponse(request, PLDM_ERROR);
    }

    return ccOnlyResponse(request, PLDM_SUCCESS);
}

/** @brief Construct the BIOS string table
 *
 *  @param[in,out] biosStringTable - the string table
 *  @param[in] request - Request message
 */
Response getBIOSStringTable(BIOSTable& biosStringTable, const pldm_msg* request)

{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    if (!biosStringTable.isEmpty())
    {
        auto rc = encode_get_bios_table_resp(
            request->hdr.instance_id, PLDM_SUCCESS,
            0, /* next transfer handle */
            PLDM_START_AND_END, nullptr, response.size(),
            responsePtr); // filling up the header here
        if (rc != PLDM_SUCCESS)
        {
            return CmdHandler::ccOnlyResponse(request, rc);
        }

        biosStringTable.load(response);
        return response;
    }
    auto biosStrings = bios_parser::getStrings();
    std::sort(biosStrings.begin(), biosStrings.end());
    // remove all duplicate strings received from bios json
    biosStrings.erase(std::unique(biosStrings.begin(), biosStrings.end()),
                      biosStrings.end());

    size_t sizeWithoutPad = std::accumulate(
        biosStrings.begin(), biosStrings.end(), 0,
        [](size_t sum, const std::string& elem) {
            return sum +
                   pldm_bios_table_string_entry_encode_length(elem.length());
        });

    Table stringTable;
    stringTable.reserve(
        pldm::responder::utils::getTableTotalsize(sizeWithoutPad));

    stringTable.resize(sizeWithoutPad);
    auto tablePtr = stringTable.data();
    for (const auto& elem : biosStrings)
    {
        auto entry_length =
            pldm_bios_table_string_entry_encode_length(elem.length());
        pldm_bios_table_string_entry_encode(tablePtr, sizeWithoutPad,
                                            elem.c_str(), elem.length());
        tablePtr += entry_length;
        sizeWithoutPad -= entry_length;
    }

    pldm::responder::utils::padAndChecksum(stringTable);
    biosStringTable.store(stringTable);
    response.resize(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                        stringTable.size(),
                    0);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    auto rc = encode_get_bios_table_resp(
        request->hdr.instance_id, PLDM_SUCCESS, 0 /* nxtTransferHandle */,
        PLDM_START_AND_END, stringTable.data(), response.size(), responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    return response;
}

namespace bios_type_enum
{

using namespace bios_parser::bios_enum;

/** @brief Find the indices  into the array of the possible values of string
 *  handles for the current values.This is used in attribute value table
 *
 *  @param[in] possiVals - vector of string handles comprising all the possible
 *                         values for an attribute
 *  @param[in] currVals - vector of strings comprising all current values
 *                        for an attribute
 *  @param[in] biosStringTable - the string table
 *
 *  @return - std::vector<uint8_t> - indices into the array of the possible
 *                                   values of string handles
 */
std::vector<uint8_t> findStrIndices(PossibleValuesByHandle possiVals,
                                    CurrentValues currVals,
                                    const BIOSStringTable& biosStringTable)
{
    std::vector<uint8_t> stringIndices;

    for (const auto& currVal : currVals)
    {
        StringHandle curHdl;
        try
        {
            curHdl = biosStringTable.findHandle(currVal);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception fetching handle for the string, STRING="
                      << currVal.c_str() << "\n";
            continue;
        }

        uint8_t i = 0;
        for (auto possiHdl : possiVals)
        {
            if (possiHdl == curHdl)
            {
                stringIndices.push_back(i);
                break;
            }
            i++;
        }
    }
    return stringIndices;
}

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
        auto index = std::find_if(possiVals.begin(), possiVals.end(),
                                  [&defs](const auto& v) { return defs == v; });
        if (index != possiVals.end())
        {
            defHdls.push_back(index - possiVals.begin());
        }
    }

    return defHdls;
}

/** @brief Construct the attibute table for BIOS type Enumeration and
 *         Enumeration ReadOnly
 *  @param[in] biosStringTable - the string table
 *  @param[in] biosJsonDir - path where the BIOS json files are present
 *  @param[in,out] attributeTable - the attribute table
 *
 */
void constructAttrTable(const BIOSStringTable& biosStringTable,
                        Table& attributeTable)
{
    const auto& attributeMap = getValues();
    StringHandle strHandle;

    for (const auto& [key, value] : attributeMap)
    {
        try
        {
            strHandle = biosStringTable.findHandle(key);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Could not find handle for BIOS string, ATTRIBUTE="
                      << key.c_str() << "\n";
            continue;
        }
        bool readOnly = (std::get<0>(value));
        const PossibleValues& possiVals = std::get<1>(value);
        const DefaultValues& defVals = std::get<2>(value);

        std::vector<StringHandle> possiValsByHdl;
        for (const auto& elem : possiVals)
        {
            try
            {
                auto hdl = biosStringTable.findHandle(elem);
                possiValsByHdl.push_back(std::move(hdl));
            }
            catch (const std::exception& e)
            {
                std::cerr << "Could not find handle for BIOS string, STRING="
                          << elem.c_str() << "\n";
                continue;
            }
        }
        auto defValsByHdl = findDefaultValHandle(possiVals, defVals);
        auto entryLength = pldm_bios_table_attr_entry_enum_encode_length(
            possiValsByHdl.size(), defValsByHdl.size());

        auto attrTableSize = attributeTable.size();
        attributeTable.resize(attrTableSize + entryLength, 0);
        struct pldm_bios_table_attr_entry_enum_info info = {
            strHandle,
            readOnly,
            (uint8_t)possiValsByHdl.size(),
            possiValsByHdl.data(),
            (uint8_t)defValsByHdl.size(),
            defValsByHdl.data(),
        };
        pldm_bios_table_attr_entry_enum_encode(
            attributeTable.data() + attrTableSize, entryLength, &info);
    }
}

void constructAttrValueEntry(
    const struct pldm_bios_attr_table_entry* attrTableEntry,
    const std::string& attrName, const BIOSStringTable& biosStringTable,
    Table& attrValueTable)
{
    CurrentValues currVals;
    try
    {
        currVals = getAttrValue(attrName);
    }
    catch (const std::exception& e)
    {
        std::cerr << "getAttrValue returned error for attribute, NAME="
                  << attrName.c_str() << " ERROR=" << e.what() << "\n";
        return;
    }
    uint8_t pv_num =
        pldm_bios_table_attr_entry_enum_decode_pv_num(attrTableEntry);
    PossibleValuesByHandle pvHdls(pv_num, 0);
    pldm_bios_table_attr_entry_enum_decode_pv_hdls(attrTableEntry,
                                                   pvHdls.data(), pv_num);
    std::sort(currVals.begin(), currVals.end());

    auto currValStrIndices = findStrIndices(pvHdls, currVals, biosStringTable);

    auto entryLength = pldm_bios_table_attr_value_entry_encode_enum_length(
        currValStrIndices.size());
    auto tableSize = attrValueTable.size();
    attrValueTable.resize(tableSize + entryLength);
    pldm_bios_table_attr_value_entry_encode_enum(
        attrValueTable.data() + tableSize, entryLength,
        attrTableEntry->attr_handle, attrTableEntry->attr_type,
        currValStrIndices.size(), currValStrIndices.data());
}

} // end namespace bios_type_enum

namespace bios_type_string
{

using namespace bios_parser::bios_string;

/** @brief Construct the attibute table for BIOS type String and
 *         String ReadOnly
 *  @param[in] biosStringTable - the string table
 *  @param[in] biosJsonDir - path where the BIOS json files are present
 *  @param[in,out] attributeTable - the attribute table
 *
 */
void constructAttrTable(const BIOSStringTable& biosStringTable,
                        Table& attributeTable)
{
    const auto& attributeMap = getValues();
    StringHandle strHandle;
    for (const auto& [key, value] : attributeMap)
    {
        try
        {
            strHandle = biosStringTable.findHandle(key);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Could not find handle for BIOS string, ATTRIBUTE="
                      << key.c_str() << "\n";
            continue;
        }

        const auto& [readOnly, strType, minStrLen, maxStrLen, defaultStrLen,
                     defaultStr] = value;
        auto entryLength =
            pldm_bios_table_attr_entry_string_encode_length(defaultStrLen);

        struct pldm_bios_table_attr_entry_string_info info = {
            strHandle, readOnly,      strType,           minStrLen,
            maxStrLen, defaultStrLen, defaultStr.data(),
        };
        auto attrTableSize = attributeTable.size();
        attributeTable.resize(attrTableSize + entryLength, 0);
        pldm_bios_table_attr_entry_string_encode(
            attributeTable.data() + attrTableSize, entryLength, &info);
    }
}

void constructAttrValueEntry(const pldm_bios_attr_table_entry* attrTableEntry,
                             const std::string& attrName,
                             const BIOSStringTable& biosStringTable,
                             Table& attrValueTable)
{
    std::ignore = biosStringTable;
    std::string currStr;
    uint16_t currStrLen = 0;
    try
    {
        currStr = getAttrValue(attrName);
        currStrLen = currStr.size();
    }
    catch (const std::exception& e)
    {
        std::cerr << "getAttrValue returned error for attribute, NAME="
                  << attrName.c_str() << " ERROR=" << e.what() << "\n";
        return;
    }
    auto entryLength =
        pldm_bios_table_attr_value_entry_encode_string_length(currStrLen);
    auto tableSize = attrValueTable.size();
    attrValueTable.resize(tableSize + entryLength);
    pldm_bios_table_attr_value_entry_encode_string(
        attrValueTable.data() + tableSize, entryLength,
        attrTableEntry->attr_handle, attrTableEntry->attr_type, currStrLen,
        currStr.c_str());
}

} // end namespace bios_type_string

namespace bios_type_integer
{

using namespace bios_parser::bios_integer;

/** @brief Construct the attibute table for BIOS type Integer and
 *         Integer ReadOnly
 *  @param[in] biosStringTable - the string table
 *  @param[in,out] attributeTable - the attribute table
 *
 */
void constructAttrTable(const BIOSStringTable& biosStringTable,
                        Table& attributeTable)
{
    const auto& attributeMap = getValues();
    StringHandle strHandle;
    for (const auto& [key, value] : attributeMap)
    {
        try
        {
            strHandle = biosStringTable.findHandle(key);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Could not find handle for BIOS string, ATTRIBUTE="
                      << key.c_str() << "\n";
            continue;
        }

        const auto& [readOnly, lowerBound, upperBound, scalarIncrement,
                     defaultValue] = value;
        auto entryLength = pldm_bios_table_attr_entry_integer_encode_length();

        struct pldm_bios_table_attr_entry_integer_info info = {
            strHandle,  readOnly,        lowerBound,
            upperBound, scalarIncrement, defaultValue,
        };
        auto attrTableSize = attributeTable.size();
        attributeTable.resize(attrTableSize + entryLength, 0);
        pldm_bios_table_attr_entry_integer_encode(
            attributeTable.data() + attrTableSize, entryLength, &info);
    }
}

void constructAttrValueEntry(const pldm_bios_attr_table_entry* attrTableEntry,
                             const std::string& attrName,
                             const BIOSStringTable& biosStringTable,
                             Table& attrValueTable)
{
    std::ignore = biosStringTable;
    uint64_t currentValue;
    try
    {
        currentValue = getAttrValue(attrName);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to get attribute value, NAME=" << attrName.c_str()
                  << " ERROR=" << e.what() << "\n";
        return;
    }
    auto entryLength = pldm_bios_table_attr_value_entry_encode_integer_length();
    auto tableSize = attrValueTable.size();
    attrValueTable.resize(tableSize + entryLength);
    pldm_bios_table_attr_value_entry_encode_integer(
        attrValueTable.data() + tableSize, entryLength,
        attrTableEntry->attr_handle, attrTableEntry->attr_type, currentValue);
}

} // namespace bios_type_integer

void traverseBIOSAttrTable(const Table& biosAttrTable,
                           AttrTableEntryHandler handler)
{
    std::unique_ptr<pldm_bios_table_iter, decltype(&pldm_bios_table_iter_free)>
        iter(pldm_bios_table_iter_create(biosAttrTable.data(),
                                         biosAttrTable.size(),
                                         PLDM_BIOS_ATTR_TABLE),
             pldm_bios_table_iter_free);
    while (!pldm_bios_table_iter_is_end(iter.get()))
    {
        auto table_entry = pldm_bios_table_iter_attr_entry_value(iter.get());
        try
        {
            handler(table_entry);
        }
        catch (const std::exception& e)
        {
            std::cerr << "handler fails when traversing BIOSAttrTable, ERROR="
                      << e.what() << "\n";
        }
        pldm_bios_table_iter_next(iter.get());
    }
}

using typeHandler = std::function<void(const BIOSStringTable& biosStringTable,
                                       Table& attributeTable)>;
std::map<BIOSJsonName, typeHandler> attrTypeHandlers{
    {bios_parser::bIOSEnumJson, bios_type_enum::constructAttrTable},
    {bios_parser::bIOSStrJson, bios_type_string::constructAttrTable},
    {bios_parser::bIOSIntegerJson, bios_type_integer::constructAttrTable},
};

/** @brief Construct the BIOS attribute table
 *
 *  @param[in,out] biosAttributeTable - the attribute table
 *  @param[in] biosStringTable - the string table
 *  @param[in] biosJsonDir - path where the BIOS json files are present
 *  @param[in] request - Request message
 */
Response getBIOSAttributeTable(BIOSTable& biosAttributeTable,
                               const BIOSStringTable& biosStringTable,
                               const char* biosJsonDir, const pldm_msg* request)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;

    if (biosAttributeTable.isEmpty())
    { // no persisted table, constructing fresh table and response
        Table attributeTable;
        fs::path dir(biosJsonDir);

        for (auto it = attrTypeHandlers.begin(); it != attrTypeHandlers.end();
             it++)
        {
            fs::path file = dir / it->first;
            if (fs::exists(file))
            {
                it->second(biosStringTable, attributeTable);
            }
        }

        if (attributeTable.empty())
        { // no available json file is found
            return CmdHandler::ccOnlyResponse(request,
                                              PLDM_BIOS_TABLE_UNAVAILABLE);
        }
        pldm::responder::utils::padAndChecksum(attributeTable);
        biosAttributeTable.store(attributeTable);
        response.resize(sizeof(pldm_msg_hdr) +
                        PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                        attributeTable.size());
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());

        auto rc = encode_get_bios_table_resp(
            request->hdr.instance_id, PLDM_SUCCESS, nxtTransferHandle,
            transferFlag, attributeTable.data(), response.size(), responsePtr);
        if (rc != PLDM_SUCCESS)
        {
            return CmdHandler::ccOnlyResponse(request, rc);
        }
    }
    else
    { // persisted table present, constructing response
        auto rc = encode_get_bios_table_resp(
            request->hdr.instance_id, PLDM_SUCCESS, nxtTransferHandle,
            transferFlag, nullptr, response.size(),
            responsePtr); // filling up the header here
        if (rc != PLDM_SUCCESS)
        {
            return CmdHandler::ccOnlyResponse(request, rc);
        }
        biosAttributeTable.load(response);
    }

    return response;
}

using AttrValTableEntryConstructHandler =
    std::function<void(const struct pldm_bios_attr_table_entry* tableEntry,
                       const std::string& attrName,
                       const BIOSStringTable& biosStringTable, Table& table)>;

using AttrType = uint8_t;
const std::map<AttrType, AttrValTableEntryConstructHandler>
    AttrValTableConstructMap{
        {PLDM_BIOS_STRING, bios_type_string::constructAttrValueEntry},
        {PLDM_BIOS_STRING_READ_ONLY, bios_type_string::constructAttrValueEntry},
        {PLDM_BIOS_ENUMERATION, bios_type_enum::constructAttrValueEntry},
        {PLDM_BIOS_ENUMERATION_READ_ONLY,
         bios_type_enum::constructAttrValueEntry},
        {PLDM_BIOS_INTEGER, bios_type_integer::constructAttrValueEntry},
        {PLDM_BIOS_INTEGER_READ_ONLY,
         bios_type_integer::constructAttrValueEntry},
    };

void constructAttrValueTableEntry(
    const struct pldm_bios_attr_table_entry* attrEntry,
    const BIOSStringTable& biosStringTable, Table& attributeValueTable)
{
    auto stringHandle =
        pldm_bios_table_attr_entry_decode_string_handle(attrEntry);
    auto attrName = biosStringTable.findString(stringHandle);
    AttrValTableConstructMap.at(attrEntry->attr_type)(
        attrEntry, attrName, biosStringTable, attributeValueTable);
}

/** @brief Construct the BIOS attribute value table
 *
 *  @param[in,out] biosAttributeValueTable - the attribute value table
 *  @param[in] biosAttributeTable - the attribute table
 *  @param[in] biosStringTable - the string table
 *  @param[in] request - Request message
 */
Response getBIOSAttributeValueTable(BIOSTable& biosAttributeValueTable,
                                    const BIOSTable& biosAttributeTable,
                                    const BIOSStringTable& biosStringTable,
                                    const pldm_msg* request)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint32_t nxtTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;

    if (!biosAttributeValueTable.isEmpty())
    {
        auto rc = encode_get_bios_table_resp(
            request->hdr.instance_id, PLDM_SUCCESS, nxtTransferHandle,
            transferFlag, nullptr, response.size(),
            responsePtr); // filling up the header here
        if (rc != PLDM_SUCCESS)
        {
            return CmdHandler::ccOnlyResponse(request, rc);
        }

        biosAttributeValueTable.load(response);
        return response;
    }

    Table attributeValueTable;
    Table attributeTable;
    biosAttributeTable.load(attributeTable);
    traverseBIOSAttrTable(
        attributeTable,
        [&biosStringTable, &attributeValueTable](
            const struct pldm_bios_attr_table_entry* tableEntry) {
            constructAttrValueTableEntry(tableEntry, biosStringTable,
                                         attributeValueTable);
        });
    if (attributeValueTable.empty())
    {
        return CmdHandler::ccOnlyResponse(request, PLDM_BIOS_TABLE_UNAVAILABLE);
    }
    pldm::responder::utils::padAndChecksum(attributeValueTable);
    biosAttributeValueTable.store(attributeValueTable);

    response.resize(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES +
                    attributeValueTable.size());
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    auto rc = encode_get_bios_table_resp(
        request->hdr.instance_id, PLDM_SUCCESS, nxtTransferHandle, transferFlag,
        attributeValueTable.data(), response.size(), responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::getBIOSTable(const pldm_msg* request, size_t payloadLength)
{
    uint32_t transferHandle{};
    uint8_t transferOpFlag{};
    uint8_t tableType{};

    auto rc = decode_get_bios_table_req(request, payloadLength, &transferHandle,
                                        &transferOpFlag, &tableType);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    auto table =
        biosConfig.getBIOSTable(static_cast<pldm_bios_table_types>(tableType));
    if (!table)
    {
        return ccOnlyResponse(request, PLDM_BIOS_TABLE_UNAVAILABLE);
    }

    Response response(sizeof(pldm_msg_hdr) +
                      PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES + table->size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    rc = encode_get_bios_table_resp(
        request->hdr.instance_id, PLDM_SUCCESS, 0 /* nxtTransferHandle */,
        PLDM_START_AND_END, table->data(), response.size(), responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::getBIOSAttributeCurrentValueByHandle(const pldm_msg* request,
                                                       size_t payloadLength)
{
    uint32_t transferHandle;
    uint8_t transferOpFlag;
    uint16_t attributeHandle;

    auto rc = decode_get_bios_attribute_current_value_by_handle_req(
        request, payloadLength, &transferHandle, &transferOpFlag,
        &attributeHandle);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    auto table = biosConfig.getBIOSTable(PLDM_BIOS_ATTR_VAL_TABLE);
    if (!table)
    {
        return ccOnlyResponse(request, PLDM_BIOS_TABLE_UNAVAILABLE);
    }

    auto entry = pldm_bios_table_attr_value_find_by_handle(
        table->data(), table->size(), attributeHandle);
    if (entry == nullptr)
    {
        return ccOnlyResponse(request, PLDM_INVALID_BIOS_ATTR_HANDLE);
    }

    auto entryLength = pldm_bios_table_attr_value_entry_length(entry);
    Response response(sizeof(pldm_msg_hdr) +
                          PLDM_GET_BIOS_ATTR_CURR_VAL_BY_HANDLE_MIN_RESP_BYTES +
                          entryLength,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    rc = encode_get_bios_current_value_by_handle_resp(
        request->hdr.instance_id, PLDM_SUCCESS, 0, PLDM_START_AND_END,
        reinterpret_cast<const uint8_t*>(entry), entryLength, responsePtr);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

Response Handler::setBIOSAttributeCurrentValue(const pldm_msg* request,
                                               size_t payloadLength)
{
    uint32_t transferHandle;
    uint8_t transferOpFlag;
    variable_field attributeField;

    auto rc = decode_set_bios_attribute_current_value_req(
        request, payloadLength, &transferHandle, &transferOpFlag,
        &attributeField);
    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    rc = biosConfig.setAttrValue(attributeField.ptr, attributeField.length);

    return ccOnlyResponse(request, rc);
}

namespace internal
{

Response buildBIOSTables(const pldm_msg* request, size_t payloadLength,
                         const char* biosJsonDir, const char* biosTablePath)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_BIOS_TABLE_MIN_RESP_BYTES,
                      0);

    if (setupConfig(biosJsonDir) != 0)
    {
        return CmdHandler::ccOnlyResponse(request, PLDM_BIOS_TABLE_UNAVAILABLE);
    }

    uint32_t transferHandle{};
    uint8_t transferOpFlag{};
    uint8_t tableType{};

    auto rc = decode_get_bios_table_req(request, payloadLength, &transferHandle,
                                        &transferOpFlag, &tableType);
    if (rc != PLDM_SUCCESS)
    {
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    BIOSTable biosStringTable(
        (std::string(biosTablePath) + "/" + stringTableFile).c_str());
    BIOSTable biosAttributeTable(
        (std::string(biosTablePath) + "/" + attrTableFile).c_str());
    BIOSTable biosAttributeValueTable(
        (std::string(biosTablePath) + "/" + attrValTableFile).c_str());
    switch (tableType)
    {
        case PLDM_BIOS_STRING_TABLE:

            response = getBIOSStringTable(biosStringTable, request);
            break;
        case PLDM_BIOS_ATTR_TABLE:

            if (biosStringTable.isEmpty())
            {
                rc = PLDM_BIOS_TABLE_UNAVAILABLE;
            }
            else
            {
                response = getBIOSAttributeTable(
                    biosAttributeTable, biosStringTable, biosJsonDir, request);
            }
            break;
        case PLDM_BIOS_ATTR_VAL_TABLE:
            if (biosAttributeTable.isEmpty() || biosStringTable.isEmpty())
            {
                rc = PLDM_BIOS_TABLE_UNAVAILABLE;
            }
            else
            {
                response = getBIOSAttributeValueTable(biosAttributeValueTable,
                                                      biosAttributeTable,
                                                      biosStringTable, request);
            }
            break;
        default:
            rc = PLDM_INVALID_BIOS_TABLE_TYPE;
            break;
    }

    if (rc != PLDM_SUCCESS)
    {
        return CmdHandler::ccOnlyResponse(request, rc);
    }

    return response;
}

} // namespace internal

} // namespace bios
} // namespace responder
} // namespace pldm
