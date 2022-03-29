#pragma once

#include "common/types.hpp"

#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

namespace pldm::fw_update::fw_inventory
{

using ObjectPath = sdbusplus::message::object_path;

using VersionIntf = sdbusplus::xyz::openbmc_project::Software::server::Version;
using AssociationIntf =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;

using Ifaces = sdbusplus::server::object::object<VersionIntf, AssociationIntf>;

/** @class Entry
 *
 */
class Entry : public Ifaces
{
  public:
    Entry() = delete;
    ~Entry() = default;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = default;
    Entry& operator=(Entry&&) = default;

    /** @brief
     *
     *  @param[in] bus  - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] version - version string
     */
    explicit Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
                   const std::string& versionStr);

    void createInventoryAssociation(const std::string& invPath);
};

class Manager
{
  public:
    const std::string softwareBasePath = "/xyz/openbmc_project/software";
    Manager() = delete;
    ~Manager() = default;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = default;
    Manager& operator=(Manager&&) = default;

    /** @brief
     *
     *  @param[in] bus  - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     */
    explicit Manager(sdbusplus::bus::bus& bus,
                     const FirmwareInventoryInfo& firmwareInventoryInfo,
                     const ComponentInfoMap& componentInfoMap);

    void createEntry(pldm::EID eid, const pldm::UUID& uuid,
                     ObjectPath objectPath);

  private:
    sdbusplus::bus::bus& bus;
    const FirmwareInventoryInfo& firmwareInventoryInfo;
    const ComponentInfoMap& componentInfoMap;
    std::map<std::pair<EID, CompIdentifier>, std::unique_ptr<Entry>>
        firmwareInventoryMap;
};

} // namespace pldm::fw_update::fw_inventory