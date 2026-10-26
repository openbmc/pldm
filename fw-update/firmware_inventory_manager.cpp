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
    const Descriptors& descriptors, const ComponentInfo& componentInfo,
    std::function<void()> taskCompletionCallback)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned seed = ts.tv_nsec ^ getpid();
    srandom(seed);

    // Determine the inventory board path for this endpoint.  If Entity-Manager
    // has provided a mapping, use it; otherwise fall back to a default
    // placeholder path so that a software object can still be created.

    auto& peer = softwareIdentifier.first;
    auto boardPath =
        getInventoryPath(peer)
            .and_then([&](const auto& path) {
                return getBoardPath(*dbusHandler, path);
            })
            .value_or(std::filesystem::path{
                "/xyz/openbmc_project/inventory/system/board/PLDM_Device"});
    const auto boardName = boardPath.filename().string();

    const auto softwarePath = std::format(
        "{}/{}_{}", SoftwareVersion::namespace_path, boardName, softwareName);
    const auto generatedId = std::to_string(utils::generateSwId());

    auto conditions = conditionConfigManager.conditions(softwareName);
    auto conditionArg = generateArg(boardName);

    updateManager.createUpdateManager(
        softwareIdentifier, descriptors, componentInfo, softwarePath,
        generatedId, conditions, conditionArg,
        std::move(taskCompletionCallback));

    softwareMap.insert_or_assign(softwareIdentifier,
                                 std::make_unique<FirmwareInventory>(
                                     softwareIdentifier, softwarePath,
                                     generatedId, activeVersion, boardPath));
}

void FirmwareInventoryManager::deleteFirmwareEntry(const PeerId& peer)
{
    std::erase_if(softwareMap,
                  [&](const auto& pair) { return pair.first.first == peer; });
    updateManager.eraseUpdateManagerIf(
        [&](const SoftwareIdentifier& softwareIdentifier) {
            return softwareIdentifier.first == peer;
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
    const PeerId& peer) const
{
    auto endpoint = registry.endpointOf(peer);
    if (!endpoint)
    {
        warning("No inventory path found, {PEER} is not a known endpoint",
                "PEER", registry.describe(peer));
        return std::nullopt;
    }

    for (const auto& [configDbusPath, configMctpInfo] : configurations)
    {
        if (toEndpoint(configMctpInfo) == *endpoint)
        {
            return configDbusPath;
        }
    }
    warning("No inventory path found for endpoint {ENDPOINT}", "ENDPOINT",
            formatEndpoint(*endpoint));
    return std::nullopt;
}

} // namespace pldm::fw_update
