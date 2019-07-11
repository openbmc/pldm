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

/** @brief Calculate the pad for BIOS tables
 *
 *  @param[in] tableLen - Length of the table
 *  @return - uint8_t - number of pad bytes
 */
uint8_t calculatePad(uint32_t tableLen)
{
    uint8_t pad;
    pad = ((tableLen % 4) ? (4 - tableLen % 4) : 0);
    return pad;
} // end calculatePad

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
                rc = PLDM_ERROR;
                break;
            case PLDM_BIOS_ATTR_VAL_TABLE:
                rc = PLDM_ERROR;
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

Response getBIOSTable(const pldm_msg* request, size_t payloadLength)
{
    const char* biosJsonDir = BIOS_JSONS_DIR;
    const char* biosTablePath = BIOS_TABLES_DIR;

    auto response =
        buildBIOSTables(request, payloadLength, biosJsonDir, biosTablePath);

    return response;
}

} // namespace responder
} // namespace pldm
