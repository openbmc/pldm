#pragma once

#include "common/types.hpp"

#include <xyz/openbmc_project/Association/Definitions/aserver.hpp>
#include <xyz/openbmc_project/Software/Version/aserver.hpp>

namespace pldm::fw_update
{

class Software;
using SoftwareVersionIntf =
    sdbusplus::aserver::xyz::openbmc_project::software::Version<Software>;
using AssociationDefinitionsIntf =
    sdbusplus::aserver::xyz::openbmc_project::association::Definitions<
        Software>;
using VersionPurpose = SoftwareVersionIntf::VersionPurpose;

using ObjectPath = pldm::dbus::ObjectPath;
using Context = sdbusplus::async::context;

class Software
{
  public:
    Software() = delete;
    Software(const Software&) = delete;
    Software(Software&&) = delete;
    Software& operator=(const Software&) = delete;
    Software& operator=(Software&&) = delete;
    ~Software() = default;

    explicit Software(Context& ctx, const std::string& path,
                      const std::string& assocEndpoint,
                      const std::string& version,
                      VersionPurpose purpose = VersionPurpose::Unknown);

  private:
    std::unique_ptr<SoftwareVersionIntf> version;
    std::unique_ptr<AssociationDefinitionsIntf> association;
};

using SoftwarePtr = std::unique_ptr<Software>;

class InventoryItemManager
{
  public:
    InventoryItemManager() = delete;
    InventoryItemManager(Context& ctx) : ctx(ctx) {}
    InventoryItemManager(const InventoryItemManager&) = delete;
    InventoryItemManager(InventoryItemManager&&) = delete;
    InventoryItemManager& operator=(const InventoryItemManager&) = delete;
    InventoryItemManager& operator=(InventoryItemManager&&) = delete;
    ~InventoryItemManager() = default;

    void createSoftwareEntry(const SoftwareIdentifier& softwareIdentifier,
                             const SoftwareName& softwareName,
                             const std::string& activeVersion);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

  private:
    std::string getVersionId(const std::string& version);

    Context& ctx;

    const ObjectPath getBoardPath(const InventoryPath& path);

    std::map<eid, InventoryPath> inventoryPathMap;

    std::map<SoftwareIdentifier, SoftwarePtr> softwareMap;
};

} // namespace pldm::fw_update
