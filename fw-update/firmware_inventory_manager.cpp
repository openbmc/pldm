#include "firmware_inventory_manager.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

void FirmwareInventoryManager::createFirmwareEntry(
    const SoftwareIdentifier& softwareIdentifier,
    const SoftwareName& softwareName, const std::string& activeVersion,
    const Descriptors& descriptors, const ComponentInfo& componentInfo)
{
    auto& eid = softwareIdentifier.first;
    const auto& inventoryPath = getInventoryPath(eid);
    if (inventoryPath.empty())
    {
        error("No inventory path found for EID {EID}", "EID", eid);
        return;
    }
    const auto boardPath = getBoardPath(inventoryPath);
    if (boardPath.empty())
    {
        error("Failed to get board path for EID {EID}", "EID", eid);
        return;
    }
    const auto boardName = boardPath.substr(boardPath.rfind('/') + 1);
    const auto boardDeviceName = boardName + "_" + softwareName;
    const auto softwarePath =
        "/xyz/openbmc_project/software/" + boardDeviceName + "_" +
        generateVersionHash(activeVersion);

    softwareMap.insert_or_assign(softwareIdentifier,
                                 std::make_unique<FirmwareInventory>(
                                     eid, softwarePath, activeVersion,
                                     boardPath, descriptors, componentInfo));
}

void FirmwareInventoryManager::deleteFirmwareEntry(const pldm::eid& eid)
{
    auto it = softwareMap.begin();
    while (it != softwareMap.end())
    {
        const auto& [identifier, _] = *it;
        const auto& [itEid, __] = identifier;
        if (itEid == eid)
        {
            it = softwareMap.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::string FirmwareInventoryManager::generateVersionHash(
    const std::string& version)
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

ObjectPath getBoardPath(const InventoryPath& path)
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
        error("Failed to get ancestors for path {PATH}: {ERROR}", "PATH", path,
              "ERROR", e);
        return {};
    }

    if (response.empty())
    {
        // Use default path if no board objects are found
        error("No Board path found for Inventory path {PATH}", "PATH", path);
        return "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    }
    if (response.size() > 1)
    {
        // Use default path if multiple or no board objects are found
        error(
            "Ambiguous Board path, found multiple Dbus Objects with the same interface {INTERFACE} for Inventory path {PATH}, size: {SIZE}",
            "INTERFACE", boardInterface, "SIZE", response.size());
        return "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    }

    return std::get<ObjectPath>(response.front());
}

InventoryPath FirmwareInventoryManager::getInventoryPath(
    const pldm::eid& eid) const
{
    for (const auto& [configDbusPath, configMctpInfo] : configurations)
    {
        if (std::get<pldm::eid>(configMctpInfo) == eid)
        {
            return configDbusPath;
        }
    }
    warning("No inventory path found for EID {EID}", "EID", eid);
    return {};
}

} // namespace pldm::fw_update
