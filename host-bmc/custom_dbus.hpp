#pragma once

#include "libpldm/pdr.h"

#include "common/utils.hpp"
#include "dbus_to_host_effecters.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/CpuCore/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/Led/Group/server.hpp>
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
using AssertedIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Led::server::Group>;

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
    void setOperationalStatus(const std::string& path, bool status);

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

    /** @brief Set the Asserted property
     *
     *  @param[in] path     - The object path
     *  @param[in] entity   - pldm entity
     *  @param[in] value    - To assert a group, set to True. To de-assert a
     *                        group, set to False.
     *  @param[in] hostEffecterParser    - Pointer to host effecter parser
     *  @param[in] instanceId - instance Id
     */
    void setAsserted(
        const std::string& path, const pldm_entity& entity, bool value,
        pldm::host_effecters::HostEffecterParser* hostEffecterParser,
        uint8_t instanceId);

    /** @brief Get the Asserted property
     *
     *  @param[in] path   - The object path
     *
     *  @return asserted  - Asserted property
     */
    bool getAsserted(const std::string& path) const;

  private:
    std::map<ObjectPath, std::unique_ptr<LocationIntf>> location;

    std::map<ObjectPath, std::unique_ptr<OperationalStatusIntf>>
        operationalStatus;

    std::unordered_map<ObjectPath, std::unique_ptr<AssertedIntf>> ledGroup;

    std::unordered_map<ObjectPath, std::unique_ptr<ItemIntf>> presentStatus;
    std::unordered_map<ObjectPath, std::unique_ptr<CoreIntf>> cpuCore;

    /** @brief Used to hold the objects which will contain EnableIface */
    std::unordered_map<ObjectPath, std::unique_ptr<EnableIface>> _enabledStatus;
};

class Group : public AssertedIntf
{
  public:
    Group() = delete;
    ~Group() = default;
    Group(const Group&) = delete;
    Group& operator=(const Group&) = delete;
    Group(Group&&) = default;
    Group& operator=(Group&&) = default;

    Group(sdbusplus::bus::bus& bus, const std::string& objPath,
          pldm::host_effecters::HostEffecterParser* hostEffecterParser,
          const pldm_entity& entity, uint8_t mctpEid) :
        AssertedIntf(bus, objPath.c_str(), true),
        hostEffecterParser(hostEffecterParser), entity(entity), mctpEid(mctpEid)
    {
        // Emit deferred signal.
        emit_object_added();
    }

    /** @brief Property SET Override function
     *
     *  @param[in]  value   -  True or False
     *  @return             -  Success or exception thrown
     */
    bool asserted(bool value) override;

  private:
    /** @brief Pointer to host effecter parser */
    pldm::host_effecters::HostEffecterParser* hostEffecterParser;

    const pldm_entity& entity;

    uint8_t mctpEid;
};

} // namespace dbus
} // namespace pldm
