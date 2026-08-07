#include "entity.hpp"

#include <common/utils.hpp>

namespace pldm
{
namespace platform_mc
{

Entity::Entity(const EntityKey& key, const std::string& path,
               const std::string& name,
               std::unique_ptr<pldm::dbus_api::PldmEntityBase> typeItemIntf) :
    key(key), path(path), typeItemIntf(std::move(typeItemIntf))
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    inventoryItemIntf = std::make_unique<InventoryItemIntf>(bus, path.c_str());
    inventoryItemIntf->prettyName(name);
    inventoryItemIntf->present(true);
}

bool Entity::addContainer(const std::string& containerPath)
{
    auto container = getContainer();
    if (container)
    {
        return std::get<2>(*container) == containerPath;
    }

    auto& bus = pldm::utils::DBusHandler::getBus();
    containerAssociationsIntf =
        std::make_unique<ContainerAssociationsIntf>(bus, path.c_str());
    containerAssociationsIntf->associations(
        {{"contained_by", "containing", containerPath}});

    return true;
}

std::optional<ContainerAssociation> Entity::getContainer() const
{
    if (!containerAssociationsIntf)
    {
        return std::nullopt;
    }

    auto definitions = containerAssociationsIntf->associations();
    if (definitions.empty())
    {
        return std::nullopt;
    }

    return definitions.front();
}

} // namespace platform_mc
} // namespace pldm
