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
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned seed = ts.tv_nsec ^ getpid();
    srandom(seed);

    auto& eid = softwareIdentifier.first;
    const auto inventoryPath = getInventoryPath(eid);
    if (!inventoryPath)
    {
        error("No inventory path found for EID {EID}", "EID", eid);
        return;
    }
    auto boardPath = boardPathProvider->getBoardPath(*inventoryPath);
    if (!boardPath)
    {
        error("Failed to get board path for EID {EID}", "EID", eid);
        return;
    }
    const auto boardName = boardPath->filename().string();
    const auto softwarePath =
        std::format("/xyz/openbmc_project/software/{}_{}_{}", boardName,
                    softwareName, generateSwId());

    softwareMap.insert_or_assign(
        softwareIdentifier, std::make_unique<FirmwareInventory>(
                                softwareIdentifier, softwarePath, activeVersion,
                                *boardPath, descriptors, componentInfo));
}

void FirmwareInventoryManager::deleteFirmwareEntry(const pldm::eid& eid)
{
    std::erase_if(softwareMap,
                  [&](const auto& pair) { return pair.first.first == eid; });
}

long int FirmwareInventoryManager::generateSwId()
{
    return random() % 10000;
}

std::optional<std::filesystem::path> BoardPathProvider::getBoardPath(
    const InventoryPath& path)
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
