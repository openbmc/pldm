#include "inventory_item_manager.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <openssl/evp.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

void InventoryItemManager::createInventoryItem(
    const eid& eid, const FirmwareDeviceName& deviceName,
    const std::string& activeVersion,
    const std::shared_ptr<UpdateManager>& updateManager)
{
    if (inventoryPathMap.contains(eid))
    {
        const auto boardPath = getBoardPath(inventoryPathMap.at(eid));
        if (boardPath.empty())
        {
            return;
        }
        auto devicePath = boardPath + "_" + deviceName;
        auto inventoryItem = std::make_unique<InventoryItemBoardIntf>(
            utils::DBusHandler::getBus(), devicePath.c_str());
        inventoryItems.emplace_back(std::move(inventoryItem));

        const auto softwarePath = "/xyz/openbmc_project/software/" +
                                  devicePath.substr(devicePath.rfind("/") + 1) +
                                  "_" + getVersionId(activeVersion);
        createVersion(softwarePath, activeVersion, VersionPurpose::Other);
        createAssociation(softwarePath, "running", "ran_on", devicePath);

        if (updateManager)
        {
            codeUpdaters.emplace_back(std::make_unique<CodeUpdater>(
                utils::DBusHandler::getBus(), softwarePath.c_str(),
                updateManager));
        }
    }
    else
    {
        error("EID {EID} not found in inventory path map", "EID", eid);
    }
}

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

void InventoryItemManager::createVersion(
    const std::string& path, std::string version, VersionPurpose purpose)
{
    if (version.empty())
    {
        version = "N/A";
    }

    auto activeSoftware = std::make_unique<Version>(
        utils::DBusHandler::getBus(), path, version, purpose);
    // auto activeSoftware = std::make_unique<SoftwareVersionIntf>(
    //     utils::DBusHandler::getBus(), path.c_str(),
    //     SoftwareVersionIntf::action::defer_emit);
    // activeSoftware->version(version);
    // activeSoftware->purpose(purpose);
    // activeSoftware->emit_object_added();
    softwareVersions.emplace_back(std::move(activeSoftware));
}

void InventoryItemManager::createAssociation(
    const std::string& path, const std::string& foward,
    const std::string& reverse, const std::string& assocEndPoint)
{
    auto association = std::make_unique<AssociationDefinitionsIntf>(
        utils::DBusHandler::getBus(), path.c_str());
    association->associations(
        {{foward.c_str(), reverse.c_str(), assocEndPoint.c_str()}});
    associations.emplace_back(std::move(association));
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
