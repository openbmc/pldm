#include "libpldm/bios.h"

#include "bios.hpp"

#include <array>
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

constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";

constexpr auto PROPERTY_INTERFACE = "org.freedesktop.DBus.Properties";

constexpr auto TIME_INTERFACE = "xyz.openbmc_project.Time.EpochTime";
constexpr auto BMC_TIME_PATH = "/xyz/openbmc_project/time/bmc";
constexpr auto DBUS_PROPERTIES = "org.freedesktop.DBus.Properties";
constexpr auto PROPERTY_ELAPSED = "Elapsed";

namespace responder
{

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface)
{
    auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetObject");

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
            throw std::runtime_error("Error reading mapper response");
        }
    }
    catch (const SdBusError& e)
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
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    auto bus = sdbusplus::bus::new_default();
    sdbusplus::message::variant<uint64_t> value;
    auto service = getService(bus, BMC_TIME_PATH, TIME_INTERFACE);

    auto method = bus.new_method_call(service.c_str(), BMC_TIME_PATH,
                                      DBUS_PROPERTIES, "Get");
    method.append(TIME_INTERFACE, PROPERTY_ELAPSED);

    auto reply = bus.call(method);
    if (reply.is_method_error())
    {
        log<level::ERR>("Error getting time",
                        entry("SERVICE=%s", service.c_str()),
                        entry("PATH=%s", BMC_TIME_PATH));

        encode_get_dateTime_resp(0, PLDM_ERROR, &seconds, &minutes, &hours,
                                 &day, &month, &year, response);
    }

    reply.read(value);
    time_usec = sdbusplus::message::variant_ns::get<uint64_t>(value);

    auto t = time_t(time_usec);
    std::string s = ctime(&t);
    formatTime(s, seconds, minutes, hours, day, month, year);

    encode_get_dateTime_resp(0, PLDM_SUCCESS, &seconds, &minutes, &hours, &day,
                             &month, &year, response);
}

} // namespace responder
} // namespace pldm
