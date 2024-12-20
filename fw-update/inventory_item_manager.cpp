#include "inventory_item_manager.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

void InventoryItemManager::createInventoryItem(
    const DeviceIdentifier& deviceIdentifier,
    const FirmwareDeviceName& deviceName, const std::string& activeVersion,
    DescriptorMap&& descriptorMap,
    DownstreamDescriptorMap&& downstreamDescriptorMap,
    ComponentInfoMap&& componentInfoMap)
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
            "/xyz/openbmc_project/software/" + boardDeviceName + "_" +
            getVersionId(activeVersion);
        auto [preConditionPath, postConditionPath] =
            jsonConditionCollector.conditions(deviceName);
        auto [itr, inserted] = aggregateUpdateManager.insert_or_assign(
            deviceIdentifier, std::move(descriptorMap),
            std::move(downstreamDescriptorMap), std::move(componentInfoMap),
            softwarePath,
            std::make_shared<FirmwareCondition>(preConditionPath, boardName),
            std::make_shared<FirmwareCondition>(
                postConditionPath, boardName,
                std::make_unique<std::function<void()>>([this, eid]() {
                    getFirmwareParameters(eid);
                })));
        auto& [_3, _4, _5, updateManager] = itr->second;
        interfaces.codeUpdater = std::make_unique<CodeUpdater>(
            utils::DBusHandler::getBus(), softwarePath, updateManager);
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

void InventoryItemManager::removeInventoryItem(
    const DeviceIdentifier& deviceIdentifier)
{
    if (interfacesMap.contains(deviceIdentifier))
    {
        interfacesMap.erase(deviceIdentifier);
    }
    if (aggregateUpdateManager.contains(deviceIdentifier))
    {
        aggregateUpdateManager.erase(deviceIdentifier);
    }
}

void InventoryItemManager::removeInventoryItems(const eid& eid)
{
    for (auto it = interfacesMap.begin(); it != interfacesMap.end();)
    {
        if (it->first.first == eid)
        {
            it = interfacesMap.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto it = aggregateUpdateManager.begin();
         it != aggregateUpdateManager.end();)
    {
        if (it->first.first == eid)
        {
            it = aggregateUpdateManager.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::string InventoryItemManager::getVersionId(const std::string& version)
{
    if (version.empty())
    {
        error("Version is empty.");
    }

    std::hash<std::string> hasher;
    auto hashValue = hasher(version);

    // We are only using the first 8 characters of the hash.
    std::stringstream ss;
    ss << std::hex << (hashValue & 0xFFFFFFFF);
    auto hashString = ss.str();

    // Ensure the hash string is exactly 8 characters long.
    if (hashString.length() > 8)
    {
        hashString = hashString.substr(0, 8);
    }
    else if (hashString.length() < 8)
    {
        hashString.insert(0, 8 - hashString.length(), '0');
    }

    return hashString;
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
