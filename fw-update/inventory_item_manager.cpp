#include "inventory_item_manager.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

void InventoryItemManager::createInventoryItem(
    const DeviceIdentifier& deviceIdentifier,
    const FirmwareDeviceName& deviceName, const std::string& activeVersion)
{
    auto& eid = deviceIdentifier.first;
    if (inventoryPathMap.contains(eid))
    {
        const auto inventoryPath = inventoryPathMap.at(eid);
        const auto boardPath = getBoardPath(inventoryPath);
        if (boardPath.empty())
        {
            error("Failed to get board path for EID {EID}", "EID", eid);
            return;
        }
        const auto boardName = boardPath.substr(boardPath.rfind("/") + 1);
        const auto boardDeviceName = boardName + "_" + deviceName;

        interfacesMap[deviceIdentifier] = {};
        auto& interfaces = interfacesMap[deviceIdentifier];

        const auto softwarePath =
            "/xyz/openbmc_project/software/" + boardDeviceName;

        createVersion(interfaces, softwarePath, activeVersion,
                      VersionPurpose::Other);
        createAssociation(interfaces, softwarePath, "running", "ran_on",
                          inventoryPath);
    }
    else
    {
        error("EID {EID} not found in inventory path map", "EID", eid);
    }
}

void InventoryItemManager::createVersion(
    InventoryItemInterfaces& interfaces, const std::string& path,
    const std::string& version, VersionPurpose purpose)
{
    const char* pathStr = path.empty() ? "N/A" : path.c_str();

    interfaces.version = std::make_unique<SoftwareVersionIntf>(ctx, pathStr);
    interfaces.version->version(version.c_str());
    interfaces.version->purpose(purpose);
}

void InventoryItemManager::createAssociation(
    InventoryItemInterfaces& interfaces, const std::string& path,
    const std::string& foward, const std::string& reverse,
    const std::string& assocEndPoint)
{
    interfaces.association =
        std::make_unique<AssociationDefinitionsIntf>(ctx, path.c_str());
    interfaces.association->associations(
        {{foward.c_str(), reverse.c_str(), assocEndPoint.c_str()}});
}

const ObjectPath InventoryItemManager::getBoardPath(const InventoryPath& path)
{
    constexpr auto boardInterface = "xyz.openbmc_project.Inventory.Item.Board";
    pldm::utils::GetAncestorsResponse response;

    try
    {
        response = pldm::utils::DBusHandler().getAncestors(path.c_str(),
                                                           {boardInterface});
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to get ancestors, error={ERROR}, path={PATH}", "ERROR", e,
              "PATH", path.c_str());
        return {};
    }

    if (response.size() != 1)
    {
        // Use default path if multiple or no board objects are found
        error(
            "Ambiguous Board path, found multiple Dbus Objects with the same interface={INTERFACE}, SIZE={SIZE}",
            "INTERFACE", boardInterface, "SIZE", response.size());
        return "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    }

    return std::get<ObjectPath>(response.front());
}

void InventoryItemManager::refreshInventoryPath(const eid& eid,
                                                const InventoryPath& path)
{
    inventoryPathMap[eid] = path;
}

} // namespace pldm::fw_update
