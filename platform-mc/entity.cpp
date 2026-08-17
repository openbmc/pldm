#include "entity.hpp"

#include <common/utils.hpp>

#include <algorithm>

namespace pldm
{
namespace platform_mc
{

Entity::Entity(const EntityKey& key, const std::string& path,
               const std::string& name,
               std::unique_ptr<pldm::dbus_api::PldmEntityBase> itemIntf) :
    key(key), path(path), itemIntf(std::move(itemIntf)),
    inventoryItemIntf(std::make_shared<InventoryItemIntf>(
        pldm::utils::DBusHandler::getBus(), path.c_str())),
    stateSets(std::make_shared<StateSets>(path, inventoryItemIntf))
{
    inventoryItemIntf->prettyName(name);
    inventoryItemIntf->present(true);
}

void Entity::addContainer(const std::string& containerPath)
{
    if (!containerAssociationsIntf)
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        containerAssociationsIntf =
            std::make_unique<ContainerAssociationsIntf>(bus, path.c_str());
    }

    ContainerAssociation definition{"contained_by", "containing",
                                    containerPath};
    auto definitions = containerAssociationsIntf->associations();
    if (std::find(definitions.begin(), definitions.end(), definition) !=
        definitions.end())
    {
        return;
    }

    definitions.emplace_back(std::move(definition));
    containerAssociationsIntf->associations(std::move(definitions));
}

std::vector<ContainerAssociation> Entity::getContainers() const
{
    if (!containerAssociationsIntf)
    {
        return {};
    }

    return containerAssociationsIntf->associations();
}

} // namespace platform_mc
} // namespace pldm
