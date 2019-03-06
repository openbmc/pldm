#include "libpldm/bios.h"

#include "bios.hpp"

#include <array>
#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace pldm
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using sdbusplus::exception::SdBusError;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr auto mapperBusName = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperInterface = "xyz.openbmc_project.ObjectMapper";
constexpr auto timeInterface = "xyz.openbmc_project.Time.EpochTime";
constexpr auto bmcTimePath = "/xyz/openbmc_project/time/bmc";
constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";
constexpr auto propertyElapsed = "Elapsed";

namespace responder
{

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface)
{
    auto mapper = bus.new_method_call(mapperBusName, mapperPath,
                                      mapperInterface, "GetObject");

    mapper.append(path, std::vector<std::string>({interface}));

    std::map<std::string, std::vector<std::string>> mapperResponse;

    try
    {
        auto mapperResponseMsg = bus.call(mapper);
        mapperResponseMsg.read(mapperResponse);

        if (mapperResponse.empty())
        {
            log<level::ERR>("Error reading mapper response",
                            entry("PATH=%s", path.c_str()),
                            entry("INTERFACE=%s", interface.c_str()));
        }
    }
    catch (const InternalFailure& e)
    {
        log<level::ERR>("Error in mapper call", entry("ERROR=%s", e.what()),
                        entry("PATH=%s", path.c_str()),
                        entry("INTERFACE=%s", interface.c_str()));
        throw;
    }
    return mapperResponse.begin()->first;
}

void getDateTime(const pldm_msg_payload* request, pldm_msg* response)
{
    uint64_t time_usec = 0;
    uint64_t time_sec = 0;
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    auto bus = sdbusplus::bus::new_default();
    sdbusplus::message::variant<uint64_t> value;
    auto service = getService(bus, bmcTimePath, timeInterface);

    auto method = bus.new_method_call(service.c_str(), bmcTimePath,
                                      dbusProperties, "Get");
    method.append(timeInterface, propertyElapsed);

    auto reply = bus.call(method);
    if (reply.is_method_error())
    {
        log<level::ERR>("Error getting time",
                        entry("SERVICE=%s", service.c_str()),
                        entry("PATH=%s", bmcTimePath));

        encode_get_date_time_resp(0, PLDM_ERROR, &seconds, &minutes, &hours,
                                  &day, &month, &year, response);
    }

    reply.read(value);
    time_usec = sdbusplus::message::variant_ns::get<uint64_t>(value);

    time_sec = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::microseconds(time_usec))
                   .count();

    auto t = time_t(time_sec);
    std::string s = ctime(&t);
    formatTime(s, seconds, minutes, hours, day, month, year);

    encode_get_date_time_resp(0, PLDM_SUCCESS, &seconds, &minutes, &hours, &day,
                              &month, &year, response);
}

} // namespace responder
} // namespace pldm
