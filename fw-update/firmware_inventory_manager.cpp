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

constexpr auto inventoryItemPlatformInterface =
    "xyz.openbmc_project.Inventory.Item.Platform";

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

/** @brief Sanitize a string for use in a D-Bus object path
 *
 *  D-Bus object paths only allow [A-Za-z0-9_]. This function replaces
 *  any invalid characters with underscores.
 *
 *  @param[in] name - The string to sanitize
 *  @return Sanitized string safe for D-Bus object paths
 */
static std::string sanitizeForDbusPath(const std::string& name)
{
    std::string result = name;
    std::replace_if(
        result.begin(), result.end(),
        [](char c) {
            return !std::isalnum(static_cast<unsigned char>(c)) && c != '_';
        },
        '_');
    return result;
}

void FirmwareInventoryManager::createFirmwareEntry(
    const SoftwareIdentifier& softwareIdentifier,
    const SoftwareName& softwareName, const std::string& activeVersion,
    const Descriptors& /*descriptors*/, const ComponentInfo& /*componentInfo*/)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned seed = ts.tv_nsec ^ getpid();
    srandom(seed);

    // Determine the inventory board path for this EID.  If Entity-Manager
    // has provided a mapping, use it; otherwise fall back to a default
    // placeholder path so that a software object can still be created.

    auto& eid = softwareIdentifier.first;
    const auto inventoryPath = getInventoryPath(eid);

    std::optional<std::filesystem::path> boardPath;
    if (inventoryPath)
    {
        boardPath = getBoardPath(*dbusHandler, *inventoryPath);
    }

    if (!boardPath)
    {
        boardPath = "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    }

    const auto boardName = boardPath->filename().string();
    const auto sanitizedName = sanitizeForDbusPath(softwareName);
    const auto softwarePath =
        std::format("{}/{}_{}_{}", SoftwareVersion::namespace_path, boardName,
                    sanitizedName, utils::generateSwId());

    softwareMap.insert_or_assign(
        softwareIdentifier,
        std::make_unique<FirmwareInventory>(softwareIdentifier, softwarePath,
                                            activeVersion, *boardPath));
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
    // Try to find an ancestor with Board or Platform interface
    static const std::array<const char*, 2> parentInterfaces = {
        InventoryItemBoard::interface, inventoryItemPlatformInterface};

    for (const auto& interface : parentInterfaces)
    {
        try
        {
            auto response = handler.getAncestors(path.c_str(), {interface});
            if (!response.empty())
            {
                return std::get<ObjectPath>(response.front());
            }
        }
        catch (const sdbusplus::exception_t& e)
        {
            error(
                "Failed to get ancestors for path {PATH} with interface {INTF}: {ERROR}",
                "PATH", path, "INTF", interface, "ERROR", e);
        }
    }

    error("No Board or Platform ancestor found for Inventory path {PATH}",
          "PATH", path);
    return std::nullopt;
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
