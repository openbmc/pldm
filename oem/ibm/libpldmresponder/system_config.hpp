#pragma once
#include "common/utils.hpp"

#include <filesystem>
#include <iostream>

namespace pldm
{
namespace responder
{
namespace oem_ibm_system_config
{
static constexpr auto compatibleInterface =
    "xyz.openbmc_project.Configuration.IBMCompatibleSystem";
static constexpr auto namesProperty = "Names";
namespace fs = std::filesystem;
class Handler
{
  public:
    Handler(const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {
        ibmCompatibleMatchConfig =
            std::make_unique<sdbusplus::bus::match::match>(
                dBusIntf->getBus(),
                sdbusplus::bus::match::rules::interfacesAdded() +
                    sdbusplus::bus::match::rules::sender(
                        "xyz.openbmc_project.EntityManager"),
                std::bind_front(&Handler::ibmCompatibleAddedCallback, this));
    }

    /** @brief Method to get the system type information
     *
     *  @return[std::filesystem::path] - the system type information
     */
    std::filesystem::path getPlatformName();

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
    void ibmCompatibleAddedCallback(sdbusplus::message::message& msg);
};

} // namespace oem_ibm_system_config
} // namespace responder
} // namespace pldm
