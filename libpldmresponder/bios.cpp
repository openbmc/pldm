#include "bios.hpp"

#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <array>
#include <chrono>
#include <ctime>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace pldm
{

using namespace phosphor::logging;

constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";

using variant = sdbusplus::message::variant<uint64_t>;

namespace responder
{

void formatTime(struct tm* time, uint8_t& seconds, uint8_t& minutes,
                uint8_t& hours, uint8_t& day, uint8_t& month, uint16_t& year)
{

    decimalToBcd<uint8_t>(time->tm_sec, seconds);
    decimalToBcd<uint8_t>(time->tm_min, minutes);
    decimalToBcd<uint8_t>(time->tm_hour, hours);
    decimalToBcd<uint8_t>(time->tm_mday, day);
    decimalToBcd<uint16_t>(time->tm_year, year);
    decimalToBcd<uint8_t>(time->tm_mon, month);
}

void getDateTime(const pldm_msg_payload* request, pldm_msg* response)
{
    uint64_t timeUsec = 0;
    uint64_t timeSec = 0;
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    constexpr auto timeInterface = "xyz.openbmc_project.Time.EpochTime";
    constexpr auto bmcTimePath = "/xyz/openbmc_project/time/bmc";

    auto bus = sdbusplus::bus::new_default();
    variant value;
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
        log<level::ERR>("Error getting time", entry("PATH=%s", bmcTimePath));

        encode_get_date_time_resp(0, PLDM_ERROR, seconds, minutes, hours, day,
                                  month, year, response);
        throw;
    }

    timeUsec = sdbusplus::message::variant_ns::get<uint64_t>(value);

    timeSec = std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::microseconds(timeUsec))
                  .count();

    auto t = time_t(timeSec);
    struct tm* time = localtime(&t);
    formatTime(time, seconds, minutes, hours, day, month, year);

    encode_get_date_time_resp(0, PLDM_SUCCESS, seconds, minutes, hours, day,
                              month, year, response);
}

} // namespace responder
} // namespace pldm
