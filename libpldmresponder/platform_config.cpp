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
        for (auto& sysTypeCallback : sysTypeCallbacks)
        {
            sysTypeCallback(systemType);
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
    static constexpr auto orgFreeDesktopInterface =
        "org.freedesktop.DBus.Properties";
    static constexpr auto getMethod = "Get";

    static constexpr auto searchpath = "/xyz/openbmc_project/";
    int depth = 0;
    std::vector<std::string> systemCompatible = {compatibleInterface};
    pldm::utils::GetSubTreeResponse response =
        pldm::utils::DBusHandler().getSubtree(searchpath, depth,
                                              systemCompatible);
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::variant<std::vector<std::string>> value;

    for (const auto& [objectPath, serviceMap] : response)
    {
        try
        {
            auto method = bus.new_method_call(
                serviceMap[0].first.c_str(), objectPath.c_str(),
                orgFreeDesktopInterface, getMethod);
            method.append(systemCompatible[0].c_str(), namesProperty);
            auto reply = bus.call(method);
            reply.read(value);
            auto systemList = std::get<std::vector<std::string>>(value);
            if (!systemList.empty())
            {
                systemType = systemList.at(0);
                return fs::path{systemType};
            }
        }
        catch (const std::exception& e)
        {
            error(
                "Error getting Names property at '{PATH}' on '{INTERFACE}': {ERROR}",
                "PATH", objectPath, "INTERFACE", systemCompatible[0], "ERROR",
                e);
        }
    }
    return std::nullopt;
}

void Handler::registerSystemTypeCallback(SystemTypeCallback callback)
{
    sysTypeCallbacks.push_back(callback);
}

} // namespace platform_config

} // namespace responder

} // namespace pldm
