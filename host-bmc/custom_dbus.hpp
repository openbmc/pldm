#pragma once

#include "common/utils.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/CpuCore/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/Object/Enable/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <map>
#include <memory>
#include <string>

namespace pldm
{
namespace dbus
{

using ObjectPath = std::string;

using LocationIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::
        LocationCode>;
using OperationalStatusIntf =
    sdbusplus::server::object::object<sdbusplus::xyz::openbmc_project::State::
                                          Decorator::server::OperationalStatus>;
using ItemIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::server::Item>;
using CoreIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::CpuCore>;
using EnableIface = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Object::server::Enable>;

/** @class CustomDBus
 *  @brief This is a custom D-Bus object, used to add D-Bus interface and
 * update the corresponding properties value.
 */
class CustomDBus
{
  private:
    CustomDBus()
    {}

  public:
    CustomDBus(const CustomDBus&) = delete;
    CustomDBus(CustomDBus&&) = delete;
    CustomDBus& operator=(const CustomDBus&) = delete;
    CustomDBus& operator=(CustomDBus&&) = delete;
    ~CustomDBus() = default;

    static CustomDBus& getCustomDBus()
    {
        static CustomDBus customDBus;
        return customDBus;
    }

  public:
    /** @brief Set the LocationCode property
     *
     *  @param[in] path  - The object path
     *
     *  @param[in] value - The value of the LocationCode property
     */
    void setLocationCode(const std::string& path, std::string value);

    /** @brief Get the LocationCode property
     *
     *  @param[in] path     - The object path
     *
     *  @return std::string - The value of the LocationCode property
     */
    std::string getLocationCode(const std::string& path) const;

    /** @brief Set the Functional property
     *
     *  @param[in] path   - The object path
     *
     *  @param[in] status - PLDM operational fault status
     */
    void setOperationalStatus(const std::string& path, uint8_t status);

    /** @brief Get the Functional property
     *
     *  @param[in] path   - The object path
     *
     *  @return status    - PLDM operational fault status
     */
    bool getOperationalStatus(const std::string& path) const;

    /** @brief Set the Inventory Item property
     *  @param[in] path - The object path
     */
    void updateItemPresentStatus(const std::string& path);

    /** @brief Implement CpuCore Interface
     *  @param[in] path - The object path
     *
     * @note This API will also implement the following interface
     *       xyz.openbmc_project.Object.Enable::Enabled dbus property
     *       which is mapped with the "Processor:Enabled" Redfish property
     *       to do either enable or disable the particular resource
     *       via Redfish client so the Enabled dbus property needs to host
     *       in the PLDM created core inventory item object.
     */
    void implementCpuCoreInterface(const std::string& path);

    /**
     * @brief Implement the xyz.openbmc_project.Object.Enable interface
     *
     * @param[in] path - The object path to implement Enable interface
     */
    void implementObjectEnableIface(const std::string& path);

  private:
    std::map<ObjectPath, std::unique_ptr<LocationIntf>> location;

    std::map<ObjectPath, std::unique_ptr<OperationalStatusIntf>>
        operationalStatus;

    std::unordered_map<ObjectPath, std::unique_ptr<ItemIntf>> presentStatus;
    std::unordered_map<ObjectPath, std::unique_ptr<CoreIntf>> cpuCore;

    /** @brief Used to hold the objects which will contain EnableIface */
    std::unordered_map<ObjectPath, std::unique_ptr<EnableIface>> _enabledStatus;
};

} // namespace dbus
} // namespace pldm
