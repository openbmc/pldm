#pragma once

#include "common/types.hpp"

#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

namespace pldm::fw_update::fw_inventory
{

using VersionIntf = sdbusplus::xyz::openbmc_project::Software::server::Version;
using AssociationIntf =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;

using Ifaces = sdbusplus::server::object::object<VersionIntf, AssociationIntf>;

/** @class Entry
 *
 *  Implementation of firmware inventory D-Bus object implementing the D-Bus
 *  interfaces.
 *
 *  a) xyz.openbmc_project.Software.Version
 *  b) xyz.openbmc_project.Association.Definitions
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

    const std::string fwdAssociation = "inventory";
    const std::string revAssociation = "activation";

    /** @brief Constructor
     *
     *  @param[in] bus  - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] version - Version string
     */
    explicit Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
                   const std::string& versionStr);

    /** @brief Create association {"inventory", "activation"} between software
     *         object and the device inventory object
     *
     *  @param[in] deviceObjPath - device inventory object
     */
    void createInventoryAssociation(const std::string& deviceObjPath);
};

/** @class Manager
 *
 *  Object manager for firmware inventory objects
 */
class Manager
{
  public:
    Manager() = delete;
    ~Manager() = default;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = default;
    Manager& operator=(Manager&&) = default;

    /** @brief Constructor
     *
     *  @param[in] bus  - Bus to attach to
     *  @param[in] firmwareInventoryInfo - Config info for firmware inventory
     *  @param[in] componentInfoMap - Component information of managed FDs
     */
    explicit Manager(sdbusplus::bus::bus& bus,
                     const FirmwareInventoryInfo& firmwareInventoryInfo,
                     const ComponentInfoMap& componentInfoMap);

    /** @brief Create firmware inventory object
     *
     *  @param[in] eid - MCTP endpointID
     *  @param[in] uuid - MCTP UUID
     *  @param[in] deviceObjPath - Object path of the device inventory object
     */
    void createEntry(pldm::EID eid, const pldm::UUID& uuid,
                     const sdbusplus::message::object_path& deviceObjPath);

    const std::string swBasePath = "/xyz/openbmc_project/software";

  private:
    sdbusplus::bus::bus& bus;

    /** @brief Config info for firmware inventory */
    const FirmwareInventoryInfo& firmwareInventoryInfo;

    /** @brief Component information needed for the update of the managed FDs */
    const ComponentInfoMap& componentInfoMap;

    /** @brief Map to store firmware inventory objects */
    std::map<std::pair<EID, CompIdentifier>, std::unique_ptr<Entry>>
        firmwareInventoryMap;
};

} // namespace pldm::fw_update::fw_inventory