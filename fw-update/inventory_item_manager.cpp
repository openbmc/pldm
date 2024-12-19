#include "inventory_item_manager.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <openssl/evp.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

void InventoryItemManager::createInventoryItem(
    const DeviceIdentifier& deviceIdentifier,
    const FirmwareDeviceName& deviceName, const std::string& activeVersion,
    DescriptorMap&& descriptorMap, ComponentInfoMap&& componentInfoMap)
{
    auto& eid = deviceIdentifier.first;
    if (inventoryPathMap.contains(eid))
    {
        const auto boardPath = getBoardPath(inventoryPathMap.at(eid));
        if (boardPath.empty())
        {
            return;
        }
        interfacesMap[deviceIdentifier] = {};
        auto& interfaces = interfacesMap[deviceIdentifier];
        auto devicePath = boardPath + "_" + deviceName;
        interfaces.board = std::make_unique<InventoryItemBoardIntf>(
            utils::DBusHandler::getBus(), devicePath.c_str());
#ifdef OPENSSL
        const auto softwarePath = "/xyz/openbmc_project/software/" +
                                  devicePath.substr(devicePath.rfind("/") + 1) +
                                  "_" + getVersionId(activeVersion);
#else
        const auto softwarePath = "/xyz/openbmc_project/software/" +
                                  devicePath.substr(devicePath.rfind("/") + 1);
#endif
        auto [itr, inserted] = aggregateUpdateManager.insert_or_assign(
            deviceIdentifier, std::move(descriptorMap),
            std::move(componentInfoMap), softwarePath);
        auto& [_3, _4, updateManager] = itr->second;
        interfaces.codeUpdater = std::make_unique<CodeUpdater>(
            utils::DBusHandler::getBus(), softwarePath, updateManager);
        createVersion(interfaces, softwarePath, activeVersion,
                      VersionPurpose::Other);
        createAssociation(interfaces, softwarePath, "running", "ran_on",
                          devicePath);
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

#ifdef OPENSSL
using EVP_MD_CTX_Ptr =
    std::unique_ptr<EVP_MD_CTX, decltype(&::EVP_MD_CTX_free)>;

std::string InventoryItemManager::getVersionId(const std::string& version)
{
    if (version.empty())
    {
        error("Version is empty.");
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    EVP_MD_CTX_Ptr ctx(EVP_MD_CTX_new(), &::EVP_MD_CTX_free);

    EVP_DigestInit(ctx.get(), EVP_sha512());
    EVP_DigestUpdate(ctx.get(), version.c_str(), strlen(version.c_str()));
    EVP_DigestFinal(ctx.get(), digest.data(), nullptr);

    // We are only using the first 8 characters.
    char mdString[9];
    snprintf(mdString, sizeof(mdString), "%02x%02x%02x%02x",
             (unsigned int)digest[0], (unsigned int)digest[1],
             (unsigned int)digest[2], (unsigned int)digest[3]);

    return mdString;
}
#endif
void InventoryItemManager::createVersion(
    InventoryItemInterfaces& interfaces, const std::string& path,
    std::string version, VersionPurpose purpose)
{
    if (version.empty())
    {
        version = "N/A";
    }

    interfaces.version = std::make_unique<Version>(utils::DBusHandler::getBus(),
                                                   path, version, purpose);
    // auto activeSoftware = std::make_unique<SoftwareVersionIntf>(
    //     utils::DBusHandler::getBus(), path.c_str(),
    //     SoftwareVersionIntf::action::defer_emit);
    // activeSoftware->version(version);
    // activeSoftware->purpose(purpose);
    // activeSoftware->emit_object_added();
}

void InventoryItemManager::createAssociation(
    InventoryItemInterfaces& interfaces, const std::string& path,
    const std::string& foward, const std::string& reverse,
    const std::string& assocEndPoint)
{
    interfaces.association = std::make_unique<AssociationDefinitionsIntf>(
        utils::DBusHandler::getBus(), path.c_str());
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
    if (inventoryPathMap.contains(eid))
    {
        inventoryPathMap.at(eid) = path;
    }
    else
    {
        inventoryPathMap.emplace(eid, path);
    }
}

} // namespace pldm::fw_update
