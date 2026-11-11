#include "firmware_inventory_manager.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/common.hpp>
#include <xyz/openbmc_project/Software/Version/common.hpp>

#include <algorithm>
#include <cctype>

using InventoryItemBoard =
    sdbusplus::common::xyz::openbmc_project::inventory::item::Board;
using SoftwareVersion =
    sdbusplus::common::xyz::openbmc_project::software::Version;

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

namespace
{

constexpr auto firmwareComponentsInterface =
    "xyz.openbmc_project.Configuration.PLDMFirmwareComponents";

std::optional<SoftwareName> getConfiguredComponentName(
    const pldm::utils::DBusHandler& handler, const InventoryPath& recordPath,
    CompIdentifier compIdentifier)
{
    pldm::utils::GetSubTreeResponse records;
    try
    {
        records = handler.getSubtree(
            std::filesystem::path(recordPath).parent_path().string(), 1,
            {firmwareComponentsInterface});
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "Failed to look up the firmware component names of {PATH}: {ERROR}",
            "PATH", recordPath, "ERROR", e);
        return std::nullopt;
    }

    const auto record =
        std::ranges::find(records, recordPath,
                          &pldm::utils::GetSubTreeResponse::value_type::first);
    if (record == records.end() || record->second.empty())
    {
        return std::nullopt;
    }

    // Each FirmwareComponents entry is an interface FirmwareComponents<N>
    const auto& [service, interfaces] = record->second.front();
    const auto entryInterface =
        std::string(firmwareComponentsInterface) + ".FirmwareComponents";
    for (const auto& interface : interfaces)
    {
        if (!interface.starts_with(entryInterface))
        {
            continue;
        }

        pldm::utils::PropertyMap entry;
        try
        {
            entry = handler.getDbusPropertiesVariant(
                service.c_str(), recordPath.c_str(), interface.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            error("Failed to read {INTERFACE} at {PATH}: {ERROR}", "INTERFACE",
                  interface, "PATH", recordPath, "ERROR", e);
            continue;
        }

        const auto identifierEntry = entry.find("ComponentIdentifier");
        const auto* identifier =
            identifierEntry == entry.end()
                ? nullptr
                : std::get_if<uint64_t>(&identifierEntry->second);
        if (identifier == nullptr || *identifier != compIdentifier)
        {
            continue;
        }

        const auto nameEntry = entry.find("Name");
        const auto* name = nameEntry == entry.end()
                               ? nullptr
                               : std::get_if<std::string>(&nameEntry->second);
        if (name == nullptr || name->empty() ||
            !std::ranges::all_of(*name, [](char c) {
                return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
            }))
        {
            error(
                "Ignoring the configured name of firmware component {IDENTIFIER} at {PATH}, it is not a valid object path element",
                "IDENTIFIER", compIdentifier, "PATH", recordPath);
            return std::nullopt;
        }
        return *name;
    }
    return std::nullopt;
}

} // namespace

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

    // Determine the inventory board path for this EID.  If Entity-Manager
    // has provided a mapping, use it; otherwise fall back to a default
    // placeholder path so that a software object can still be created.

    auto& eid = softwareIdentifier.first;
    const auto inventoryPath = getInventoryPath(eid);
    auto boardPath =
        inventoryPath
            .and_then([&](const auto& path) {
                return getBoardPath(*dbusHandler, path);
            })
            .value_or(std::filesystem::path{
                "/xyz/openbmc_project/inventory/system/board/PLDM_Device"});
    const auto boardName = boardPath.filename().string();

    // A configured name must be unique in the system, so it omits the board
    const auto configuredName = inventoryPath.and_then([&](const auto& path) {
        return getConfiguredComponentName(*dbusHandler, path,
                                          softwareIdentifier.second);
    });
    const auto name = configuredName.value_or(softwareName);
    const auto softwarePath =
        configuredName
            ? std::format("{}/{}", SoftwareVersion::namespace_path, name)
            : std::format("{}/{}_{}", SoftwareVersion::namespace_path,
                          boardName, name);
    const auto generatedId = std::to_string(utils::generateSwId());

    auto conditions = conditionConfigManager.conditions(name);
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
