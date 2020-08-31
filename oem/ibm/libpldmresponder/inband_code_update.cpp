#include "inband_code_update.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Dump/NewDump/server.hpp>

#include <exception>

namespace pldm
{

namespace responder
{

std::string CodeUpdate::fetchCurrentBootSide()
{
    return currBootSide;
}

std::string CodeUpdate::fetchNextBootSide()
{
    return nextBootSide;
}

int CodeUpdate::setCurrentBootSide(std::string currSide)
{
    currBootSide = currSide;
    return PLDM_SUCCESS;
}

int CodeUpdate::setNextBootSide(std::string nextSide)
{
    nextBootSide = nextSide;
    // change the priority property of version
    return PLDM_SUCCESS;
}

void CodeUpdate::setVersions()
{
    static constexpr auto MAPPER_SERVICE = "xyz.openbmc_project.ObjectMapper";
    static constexpr auto FUNCTIONAL_OBJ_PATH =
        "/xyz/openbmc_project/software/functional";
    static constexpr auto ACTIVE_OBJ_PATH =
        "/xyz/openbmc_project/software/active";
    static constexpr auto PROP_INTF = "org.freedesktop.DBus.Properties";

    auto& bus = dBusIntf->getBus();

    try
    {
        auto method = bus.new_method_call(MAPPER_SERVICE, FUNCTIONAL_OBJ_PATH,
                                          PROP_INTF, "Get");
        method.append("xyz.openbmc_project.Association", "endpoints");

        auto reply = bus.call(method);
        reply.read(runningVersion);

        std::vector<std::string> paths;
        auto method1 = bus.new_method_call(MAPPER_SERVICE, ACTIVE_OBJ_PATH,
                                           PROP_INTF, "Get");
        method1.append("xyz.openbmc_project.Association", "endpoints");

        auto reply1 = bus.call(method1);
        reply.read(paths);
        for (const auto& path : paths)
        {
            if (path != runningVersion)
            {
                nonRunningVersion = path;
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to make a d-bus call to Object Mapper "
                     "Association, ERROR="
                  << e.what() << "\n";
        return;
    }
}

} // namespace responder

} // namespace pldm
