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
    stateSets(std::make_shared<StateSets>(path))
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    inventoryItemIntf = std::make_unique<InventoryItemIntf>(bus, path.c_str());
    inventoryItemIntf->prettyName(name);
    inventoryItemIntf->present(true);
}

void Entity::addAssociation(const std::string& forward,
                            const std::string& reverse,
                            const std::string& endpointPath)
{
    if (!associationsIntf)
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        associationsIntf =
            std::make_unique<AssociationsIntf>(bus, path.c_str());
    }

    AssociationDefinition definition{forward, reverse, endpointPath};
    auto definitions = associationsIntf->associations();
    if (std::find(definitions.begin(), definitions.end(), definition) !=
        definitions.end())
    {
        return;
    }

    definitions.emplace_back(std::move(definition));
    associationsIntf->associations(std::move(definitions));
}

std::vector<AssociationDefinition> Entity::getAssociations(
    const std::string& forward) const
{
    if (!associationsIntf)
    {
        return {};
    }

    auto definitions = associationsIntf->associations();
    std::erase_if(definitions, [&forward](const AssociationDefinition& def) {
        return std::get<0>(def) != forward;
    });

    return definitions;
}

void Entity::addContainer(const std::string& containerPath)
{
    addAssociation("contained_by", "containing", containerPath);
}

std::vector<AssociationDefinition> Entity::getContainers() const
{
    return getAssociations("contained_by");
}

} // namespace platform_mc
} // namespace pldm
