#pragma once

#include "xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/AssetTag/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/Compatible/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/Revision/server.hpp"
#include "xyz/openbmc_project/Inventory/Item/Board/server.hpp"
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>

#include <map>

namespace pldm
{
namespace dbus_api
{

using assetserver =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset;
using assettagserver =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::AssetTag;
using revisionserver =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Revision;
using compatibleserver =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Compatible;
using boardserver =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Board;
using versionserver =
    sdbusplus::xyz::openbmc_project::Software::server::Version;
using definitionsserver =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using assetserver =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset;

using AssetIntf = sdbusplus::server::object_t<assetserver>;
using AssetTagIntf = sdbusplus::server::object_t<assettagserver>;
using RevisionIntf = sdbusplus::server::object_t<revisionserver>;
using CompatibleIntf = sdbusplus::server::object_t<compatibleserver>;
using BoardIntf = sdbusplus::server::object_t<boardserver>;
using VersionInterface = sdbusplus::server::object_t<versionserver>;
using DefinitionsInterface = sdbusplus::server::object_t<definitionsserver>;
using AssetInterface = sdbusplus::server::object_t<assetserver>;

/** @class PldmEntityRequester
 *  @brief OpenBMC PLDM Inventory entity implementation.
 *  @details A concrete implementation for the PLDM Inventory entity DBus APIs.
 */
class PldmEntityReq :
    public AssetIntf,
    public AssetTagIntf,
    public RevisionIntf,
    public CompatibleIntf,
    public BoardIntf
{
  public:
    PldmEntityReq() = delete;
    PldmEntityReq(const PldmEntityReq&) = delete;
    PldmEntityReq& operator=(const PldmEntityReq&) = delete;
    PldmEntityReq(PldmEntityReq&&) = delete;
    PldmEntityReq& operator=(PldmEntityReq&&) = delete;
    virtual ~PldmEntityReq() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    PldmEntityReq(sdbusplus::bus_t& bus, const std::string& path) :
        AssetIntf(bus, path.c_str()), AssetTagIntf(bus, path.c_str()),
        RevisionIntf(bus, path.c_str()), CompatibleIntf(bus, path.c_str()),
        BoardIntf(bus, path.c_str()) {};

    /** @brief Set value of partNumber in Decorator.Asset */
    std::string partNumber(std::string value);

    /** @brief Set value of serialNumber in Decorator.Asset */
    std::string serialNumber(std::string value);

    /** @brief Set value of manufacturer in Decorator.Asset */
    std::string manufacturer(std::string value);

    /** @brief Set value of buildDate in Decorator.Asset */
    std::string buildDate(std::string value);

    /** @brief Set value of model in Decorator.Asset */
    std::string model(std::string value);

    /** @brief Set value of subModel in Decorator.Asset */
    std::string subModel(std::string value);

    /** @brief Set value of sparePartNumber in Decorator.Asset */
    std::string sparePartNumber(std::string value);

    /** @brief Set value of assetTag in Decorator.AssetTag */
    std::string assetTag(std::string value);

    /** @brief Set value of version in Decorator.Revision */
    std::string version(std::string value);

    /** @brief Set value of names in in Decorator.Compatible */
    std::vector<std::string> names(std::vector<std::string> values);
};

/** @class SoftwareInventory
 *  @brief OpenBMC Software Inventory implementation.
 *  @details Exposes PLDM firmware versions on DBus.
 */
class SoftwareInventory : public VersionInterface,
                         public DefinitionsInterface,
                         public AssetInterface
{
  public:
    SoftwareInventory() = delete;
    SoftwareInventory(const SoftwareInventory&) = delete;
    SoftwareInventory& operator=(const SoftwareInventory&) = delete;
    SoftwareInventory(SoftwareInventory&&) = delete;
    SoftwareInventory& operator=(SoftwareInventory&&) = delete;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    SoftwareInventory(sdbusplus::bus_t& bus, const std::string& path) :
    VersionInterface(bus, path.c_str()),
    DefinitionsInterface(bus, path.c_str()),
    AssetInterface(bus, path.c_str()) {};

    /** @brief Set value of Purpose in Software.Version */
    versionserver::VersionPurpose purpose(versionserver::VersionPurpose value);

    /** @brief Set value of Version in Software.Version */
    std::string version(std::string value);

};

} // namespace dbus_api
} // namespace pldm
