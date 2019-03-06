#include "utils.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <array>
#include <ctime>
#include <iostream>
#include <map>
#include <phosphor-logging/elog-errors.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace pldm
{
using namespace phosphor::logging;

constexpr auto mapperBusName = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperInterface = "xyz.openbmc_project.ObjectMapper";

using Interface = std::vector<std::string>;

namespace responder
{

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface)
{
    std::map<std::string, std::vector<std::string>> mapperResponse;

    try
    {
        auto mapper = bus.new_method_call(mapperBusName, mapperPath,
                                          mapperInterface, "GetObject");
        mapper.append(path, Interface({interface}));

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

} // namespace responder
} // namespace pldm
