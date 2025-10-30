#include "firmware_inventory_manager.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/common.hpp>
#include <xyz/openbmc_project/Software/Version/common.hpp>

using InventoryItemBoard =
    sdbusplus::common::xyz::openbmc_project::inventory::item::Board;
using SoftwareVersion =
    sdbusplus::common::xyz::openbmc_project::software::Version;

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

void FirmwareInventoryManager::createFirmwareEntry(
    const SoftwareIdentifier& softwareIdentifier,
    const SoftwareName& softwareName, const std::string& activeVersion,
    const Descriptors& descriptors, const ComponentInfo& componentInfo)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned seed = ts.tv_nsec ^ getpid();
    srandom(seed);

    // Determine the inventory board path for this EID.  If Entity-Manager
    // has provided a mapping, use it; otherwise fall back to a default
    // placeholder path so that a software object can still be created.

    std::optional<std::filesystem::path> boardPath;

    auto& eid = softwareIdentifier.first;
    const auto inventoryPath = getInventoryPath(eid);
    if (inventoryPath)
    {
        boardPath = getBoardPath(*dbusHandler, *inventoryPath);
    }
    else
    {
        boardPath = "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    }
    const auto boardName = boardPath->filename().string();

    const auto softwarePath = std::format(
        "{}/{}_{}", SoftwareVersion::namespace_path, boardName, softwareName);
    const auto softwareHash = std::to_string(utils::generateSwId());

    updateManager.createUpdateManager(softwareIdentifier, descriptors,
                                      componentInfo, softwarePath,
                                      softwareHash);

    softwareMap.insert_or_assign(softwareIdentifier,
                                 std::make_unique<FirmwareInventory>(
                                     softwareIdentifier, softwarePath,
                                     softwareHash, activeVersion, *boardPath));
}

void FirmwareInventoryManager::deleteFirmwareEntry(const pldm::eid& eid)
{
    std::erase_if(softwareMap,
                  [&](const auto& pair) { return pair.first.first == eid; });
    updateManager.eraseUpdateManagerIf(
        [&](const SoftwareIdentifier& softwareIdentifier) {
            return softwareIdentifier.first == eid;
        });
}

std::optional<std::filesystem::path> getBoardPath(
    const pldm::utils::DBusHandler& handler, const InventoryPath& path)
{
    pldm::utils::GetAncestorsResponse response;

    try
    {
        response =
            handler.getAncestors(path.c_str(), {InventoryItemBoard::interface});
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to get ancestors for path {PATH}: {ERROR}", "PATH", path,
              "ERROR", e);
        return std::nullopt;
    }

    if (response.empty())
    {
        error(
            "Failed to get unique board path for Inventory path {PATH}, found: {SIZE}",
            "PATH", path, "SIZE", response.size());
        return std::nullopt;
    }

    return std::get<ObjectPath>(response.front());
}

std::optional<InventoryPath> FirmwareInventoryManager::getInventoryPath(
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
    return std::nullopt;
}

} // namespace pldm::fw_update
