#include "platform_config.hpp"

namespace pldm
{
namespace responder
{

namespace platform_config
{
/** @brief callback function invoked when interfaces get added from
 *      Entity manager
 *
 *  @param[in] msg - Data associated with subscribed signal
 */
void Handler::systemCompatibleCallback(sdbusplus::message_t& msg)
{
    sdbusplus::message::object_path path;

    pldm::utils::InterfaceMap interfaceMap;

    msg.read(path, interfaceMap);

    if (!interfaceMap.contains(compatibleInterface))
    {
        return;
    }
    // Get the "Name" property value of the
    // "xyz.openbmc_project.Inventory.Decorator.Compatible" interface
    const auto& properties = interfaceMap.at(compatibleInterface);

    if (!properties.contains(namesProperty))
    {
        return;
    }
    auto names =
        std::get<pldm::utils::Interfaces>(properties.at(namesProperty));

    std::string systemType;
    if (!names.empty())
    {
        // get only the first system type
        systemType = names.front();
        if (sysTypeCallback)
        {
            sysTypeCallback(systemType, true);
        }
    }

    if (!systemType.empty())
    {
        systemCompatibleMatchCallBack.reset();
    }
}

/** @brief Method to get the system type information
 *
 *  @return - the system type information
 */
std::optional<std::filesystem::path> Handler::getPlatformName()
{
    if (!systemType.empty())
    {
        return fs::path{systemType};
    }

    namespace fs = std::filesystem;
    static const std::string entityMangerService =
        "xyz.openbmc_project.EntityManager";

    static constexpr auto searchpath = "/xyz/openbmc_project/";
    int depth = 0;
    std::vector<std::string> systemCompatible = {compatibleInterface};

    try
    {
        pldm::utils::GetSubTreeResponse response =
            pldm::utils::DBusHandler().getSubtree(searchpath, depth,
                                                  systemCompatible);
        auto& bus = pldm::utils::DBusHandler::getBus();

        for (const auto& [objectPath, serviceMap] : response)
        {
            try
            {
                auto record = std::find_if(
                    serviceMap.begin(), serviceMap.end(),
                    [](auto map) { return map.first == entityMangerService; });

                if (record != serviceMap.end())
                {
                    auto method = bus.new_method_call(
                        entityMangerService.c_str(), objectPath.c_str(),
                        "org.freedesktop.DBus.Properties", "Get");
                    method.append(compatibleInterface, namesProperty);
                    auto propSystemList =
                        bus.call(method, dbusTimeout).unpack<PropertyValue>();
                    auto systemList =
                        std::get<std::vector<std::string>>(propSystemList);

                    if (!systemList.empty())
                    {
                        systemType = systemList.at(0);
                        // once systemtype received,then resetting a callback
                        systemCompatibleMatchCallBack.reset();
                        return fs::path{systemType};
                    }
                }
            }
            catch (const std::exception& e)
            {
                error(
                    "Error getting Names property at '{PATH}' on '{INTERFACE}': {ERROR}",
                    "PATH", objectPath, "INTERFACE", compatibleInterface,
                    "ERROR", e);
            }
        }
    }
    catch (const std::exception& e)
    {
        error("Failed to make a d-bus call to get platform name {ERROR}",
              "ERROR", e);
    }
    return std::nullopt;
}

void Handler::registerSystemTypeCallback(SystemTypeCallback callback)
{
    sysTypeCallback = callback;
}

} // namespace platform_config

} // namespace responder

} // namespace pldm
