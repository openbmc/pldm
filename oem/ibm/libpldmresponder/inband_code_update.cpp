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
    int rc = PLDM_SUCCESS;

    nextBootSide = nextSide;

    static constexpr auto PROP_INTF =
        "xyz.openbmc_project.Software.RedundancyPriority";
    std::string obj_path{};
    if (nextBootSide == currBootSide)
    {
        obj_path = runningVersion;
    }
    else
    {
        obj_path = nonRunningVersion;
    }

    pldm::utils::DBusMapping dbusMapping{obj_path, PROP_INTF, "Priority",
                                         "uint8_t"};
    pldm::utils::PropertyValue value{0};
    try
    {
        dBusIntf->setDbusProperty(dbusMapping, value);
    }
    catch (const std::exception& e)
    {
        std::cerr << "failed to set the next boot side to " << obj_path.c_str()
                  << " ERROR=" << e.what() << "\n";
        rc = PLDM_ERROR;
    }
    return rc;
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

    using namespace sdbusplus::bus::match::rules;
    static constexpr auto PROP_CHANGE_INTF =
        "xyz.openbmc_project.Software.RedundancyPriority";
    captureNextBootSideChange.push_back(
        std::make_unique<sdbusplus::bus::match::match>(
            pldm::utils::DBusHandler::getBus(),
            propertiesChanged(runningVersion, PROP_CHANGE_INTF),
            [this](sdbusplus::message::message& msg) {
                DbusChangedProps props;
                std::string iface;
                msg.read(iface, props);
                processPriorityChangeNotification(props);
            }));
}

void CodeUpdate::processPriorityChangeNotification(
    const DbusChangedProps& chProperties)
{
    static constexpr auto propName = "Priority";
    const auto it = chProperties.find(propName);
    if (it == chProperties.end())
    {
        return;
    }
    uint8_t newVal = std::get<uint8_t>(it->second);
    nextBootSide = (newVal == 0) ? currBootSide
                                 : ((currBootSide == Tside) ? Pside : Tside);
}

} // namespace responder

} // namespace pldm
