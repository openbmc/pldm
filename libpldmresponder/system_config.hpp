#pragma once
#include "common/utils.hpp"

#include <filesystem>
#include <iostream>

namespace pldm
{
namespace responder
{
static constexpr auto compatibleInterface =
    "xyz.openbmc_project.Configuration.IBMCompatibleSystem";
static constexpr auto namesProperty = "Names";
namespace fs = std::filesystem;
class SystemConfig
{
  public:
    SystemConfig() = delete;
    SystemConfig(const SystemConfig&) = delete;
    SystemConfig(SystemConfig&&) = delete;
    SystemConfig& operator=(const SystemConfig&) = delete;
    SystemConfig& operator=(SystemConfig&&) = delete;
    ~SystemConfig() = default;

    SystemConfig(const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {
        ibmCompatibleMatchConfig =
            std::make_unique<sdbusplus::bus::match::match>(
                dBusIntf->getBus(),
                sdbusplus::bus::match::rules::interfacesAdded() +
                    sdbusplus::bus::match::rules::sender(
                        "xyz.openbmc_project.EntityManager"),
                std::bind_front(&SystemConfig::ibmCompatibleAddedCallback,
                                this));
    }

    /** @brief Method to get the system type information
     *
     *  @return[std::filesystem::path] - the system type information
     */
    std::filesystem::path getPlatformName()
    {
        if (!systemType.empty())
        {
            return fs::path{systemType};
        }

        static constexpr auto searchpath = "/xyz/openbmc_project/";
        int depth = 0;
        std::vector<std::string> ibmCompatible = {compatibleInterface};
        pldm::utils::GetSubTreeResponse response;
        try
        {
            response = pldm::utils::DBusHandler().getSubtree(searchpath, depth,
                                                             ibmCompatible);
        }
        catch (const sdbusplus::exception_t& e)
        {
            std::cerr << "getSubtree call failed with, ERROR=" << e.what()
                      << ", PATH=" << searchpath
                      << ", INTERFACE=" << ibmCompatible[0] << "\n";
            return fs::path();
        }

        for (const auto& [objectPath, serviceMap] : response)
        {
            try
            {
                auto value = pldm::utils::DBusHandler()
                                 .getDbusProperty<std::vector<std::string>>(
                                     objectPath.c_str(), namesProperty,
                                     ibmCompatible[0].c_str());
                return fs::path{value[0]};
            }
            catch (const sdbusplus::exception_t& e)
            {
                std::cerr << "Error getting Names property , ERROR=" << e.what()
                          << ", PATH=" << objectPath
                          << ", INTERFACE =" << ibmCompatible[0] << "\n";
            }
        }
        return fs::path();
    }

  private:
    /**@ brief system type/model */
    std::string systemType;

    /** @brief D-Bus Interfaced added signal match for Entity Manager */
    std::unique_ptr<sdbusplus::bus::match::match> ibmCompatibleMatchConfig;

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;

    /** @brief callback function invoked when interfaces get added from
     *     Entity manager
     *
     *  @param[in] msg - Data associated with subscribed signal
     */
    void ibmCompatibleAddedCallback(sdbusplus::message::message& msg)
    {
        sdbusplus::message::object_path path;

        pldm::utils::InterfacesMap interfaceMap;

        msg.read(path, interfaceMap);

        if (!interfaceMap.contains(compatibleInterface))
        {
            return;
        }
        // Get the "Name" property value of the
        // "xyz.openbmc_project.Configuration.IBMCompatibleSystem" interface
        const auto& properties = interfaceMap.at(compatibleInterface);

        if (!properties.contains(namesProperty))
        {
            return;
        }
        auto names =
            std::get<pldm::utils::Interfaces>(properties.at(namesProperty));

        // get only the first system type
        if (!names.empty())
        {
            systemType = names.front();
        }

        if (!systemType.empty())
        {
            ibmCompatibleMatchConfig.reset();
        }
    }
};

} // namespace responder
} // namespace pldm
