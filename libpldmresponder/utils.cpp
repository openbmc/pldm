#include "utils.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <array>
#include <ctime>
#include <iostream>
#include <map>
#include <phosphor-logging/log.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace pldm
{
using namespace phosphor::logging;

constexpr auto mapperBusName = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperInterface = "xyz.openbmc_project.ObjectMapper";

namespace responder
{

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface)
{
    using DbusInterfaceList = std::vector<std::string>;
    std::map<std::string, std::vector<std::string>> mapperResponse;

    try
    {
        auto mapper = bus.new_method_call(mapperBusName, mapperPath,
                                          mapperInterface, "GetObject");
        mapper.append(path, DbusInterfaceList({interface}));

        auto mapperResponseMsg = bus.call(mapper);
        mapperResponseMsg.read(mapperResponse);
    }
    catch (std::exception& e)
    {
        log<level::ERR>("Error in mapper call", entry("ERROR=%s", e.what()),
                        entry("PATH=%s", path.c_str()),
                        entry("INTERFACE=%s", interface.c_str()));
        throw;
    }
    return mapperResponse.begin()->first;
}

namespace utils
{

uint8_t bcd2dec8(uint8_t bcd)
{
    uint8_t dec = (bcd >> 4) * 10 + (bcd & 0x0f);
    return dec;
}

uint16_t bcd2dec16(uint16_t bcd)
{
    return bcd2dec8(bcd >> 8) * 100 + bcd2dec8(bcd & 0xff);
}

uint8_t getNumPadBytes(uint32_t data)
{
    uint8_t pad;
    pad = ((data % 4) ? (4 - data % 4) : 0);
    return pad;
} // end getNumPadBytes

} // end namespace utils
} // namespace responder
} // namespace pldm
