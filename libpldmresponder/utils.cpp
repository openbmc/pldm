#include "utils.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <array>
#include <ctime>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace pldm
{

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
        std::cerr << "Error in mapper call, ERROR=" << e.what()
                  << " PATH=" << path.c_str()
                  << " INTERFACE=" << interface.c_str() << std::endl;
        throw;
    }
    return mapperResponse.begin()->first;
}

void reportError(const std::string& failureType)
{
    static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
    static constexpr auto logInterface = "xyz.openbmc_project.Logging.Create";

    static sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

    try
    {
        auto service = getService(bus, logObjPath, logInterface);
        using namespace sdbusplus::xyz::openbmc_project::Logging::server;
        auto severity =
            sdbusplus::xyz::openbmc_project::Logging::server::convertForMessage(
                sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level::
                    Error);
        auto method = bus.new_method_call(service.c_str(), logObjPath,
                                          logInterface, "Create");
        method.append(failureType.c_str(), severity);
        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to create error log, ERROR="
                  << e.what() << std::endl;
    }
}

namespace utils
{

uint8_t getNumPadBytes(uint32_t data)
{
    uint8_t pad;
    pad = ((data % 4) ? (4 - data % 4) : 0);
    return pad;
} // end getNumPadBytes

} // end namespace utils
} // namespace responder
} // namespace pldm
