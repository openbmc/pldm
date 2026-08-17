#pragma once

#include "common/types.hpp"
#include "dbus_impl_fru.hpp"
#include "state_set.hpp"

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using namespace pldm::pdr;

using ContainerAssociationsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
using ContainerAssociation = std::tuple<std::string, std::string, std::string>;

/**
 * @brief Entity
 *
 * The D-Bus object of one PLDM entity of a terminus. The object implements
 * xyz.openbmc_project.Inventory.Item, the Inventory.Item interface which
 * matches the entity type, and the containment association to the entity
 * which contains it. It also holds the state set interfaces through which the
 * state sensors of the entity publish the state of the entity.
 */
class Entity
{
  public:
    Entity() = delete;
    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) = delete;
    Entity& operator=(Entity&&) = delete;
    ~Entity() = default;

    /** @brief Constructor
     *
     *  @param[in] key - the entity identification fields of the PDR
     *  @param[in] path - the D-Bus object path of the entity
     *  @param[in] name - the entity name
     *  @param[in] itemIntf - the Inventory.Item interface which matches the
     *                        entity type
     */
    Entity(const EntityKey& key, const std::string& path,
           const std::string& name,
           std::unique_ptr<pldm::dbus_api::PldmEntityBase> itemIntf);

    /** @brief The getter to return the entity identification fields */
    const EntityKey& getKey() const
    {
        return key;
    }

    /** @brief The getter to return the entity D-Bus object path */
    const std::string& getPath() const
    {
        return path;
    }

    /** @brief Add the containment association to the entity which contains
     *         this entity
     *
     *  @param[in] containerPath - D-Bus object path of the container entity
     */
    void addContainer(const std::string& containerPath);

    /** @brief The getter to return the containment associations of the entity
     */
    std::vector<ContainerAssociation> getContainers() const;

    /** @brief The getter to return the state set interfaces implemented on
     *         the D-Bus object of the entity
     */
    std::shared_ptr<StateSets> getStateSets() const
    {
        return stateSets;
    }

  private:
    /** @brief The entity identification fields of the PDR */
    EntityKey key;

    /** @brief The D-Bus object path of the entity */
    std::string path;

    /** @brief The pointer of the Inventory.Item interface of the entity type
     */
    std::unique_ptr<pldm::dbus_api::PldmEntityBase> itemIntf;

    /** @brief The pointer of the Inventory.Item interface, which the state
     *         sets that publish on it share
     */
    std::shared_ptr<InventoryItemIntf> inventoryItemIntf;

    /** @brief The pointer of the Association.Definitions interface */
    std::unique_ptr<ContainerAssociationsIntf> containerAssociationsIntf;

    /** @brief The state set interfaces of the entity */
    std::shared_ptr<StateSets> stateSets;
};

} // namespace platform_mc
} // namespace pldm
