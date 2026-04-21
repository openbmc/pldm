#pragma once

#include "xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/AssetTag/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/Compatible/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/Revision/server.hpp"
#include "xyz/openbmc_project/Inventory/Item/Board/server.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>

#include <string>
#include <vector>

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

using BoardIntf = sdbusplus::server::object_t<boardserver>;

/** @class PldmBoardEntity
 *  @brief Inventory.Item.Board D-Bus interface for a PLDM terminus.
 */
class PldmBoardEntity : public BoardIntf
{
  public:
    PldmBoardEntity() = delete;
    PldmBoardEntity(const PldmBoardEntity&) = delete;
    PldmBoardEntity& operator=(const PldmBoardEntity&) = delete;
    PldmBoardEntity(PldmBoardEntity&&) = delete;
    PldmBoardEntity& operator=(PldmBoardEntity&&) = delete;
    ~PldmBoardEntity() override = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    PldmBoardEntity(sdbusplus::bus_t& bus, const std::string& path) :
        BoardIntf(bus, path.c_str()) {};
};

using AssetIntf = sdbusplus::server::object_t<assetserver>;
using AssetTagIntf = sdbusplus::server::object_t<assettagserver>;
using RevisionIntf = sdbusplus::server::object_t<revisionserver>;
using CompatibleIntf = sdbusplus::server::object_t<compatibleserver>;

/** @class PldmFruDecorators
 *  @brief FRU decorator D-Bus interfaces for a PLDM terminus.
 *  @details Created only when the terminus provides valid FRU record data,
 *           so that termini without FRU support do not expose empty decorator
 *           interfaces on D-Bus.
 */
class PldmFruDecorators :
    public AssetIntf,
    public AssetTagIntf,
    public RevisionIntf,
    public CompatibleIntf
{
  public:
    PldmFruDecorators() = delete;
    PldmFruDecorators(const PldmFruDecorators&) = delete;
    PldmFruDecorators& operator=(const PldmFruDecorators&) = delete;
    PldmFruDecorators(PldmFruDecorators&&) = delete;
    PldmFruDecorators& operator=(PldmFruDecorators&&) = delete;
    ~PldmFruDecorators() override = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    PldmFruDecorators(sdbusplus::bus_t& bus, const std::string& path) :
        AssetIntf(bus, path.c_str()), AssetTagIntf(bus, path.c_str()),
        RevisionIntf(bus, path.c_str()), CompatibleIntf(bus, path.c_str()) {};

    /** @brief Set value of partNumber in Decorator.Asset */
    std::string partNumber(std::string value) override;

    /** @brief Set value of serialNumber in Decorator.Asset */
    std::string serialNumber(std::string value) override;

    /** @brief Set value of manufacturer in Decorator.Asset */
    std::string manufacturer(std::string value) override;

    /** @brief Set value of buildDate in Decorator.Asset */
    std::string buildDate(std::string value) override;

    /** @brief Set value of model in Decorator.Asset */
    std::string model(std::string value) override;

    /** @brief Set value of subModel in Decorator.Asset */
    std::string subModel(std::string value) override;

    /** @brief Set value of sparePartNumber in Decorator.Asset */
    std::string sparePartNumber(std::string value) override;

    /** @brief Set value of assetTag in Decorator.AssetTag */
    std::string assetTag(std::string value) override;

    /** @brief Set value of version in Decorator.Revision */
    std::string version(std::string value) override;

    /** @brief Set value of names in Decorator.Compatible */
    std::vector<std::string> names(std::vector<std::string> values) override;
};

} // namespace dbus_api
} // namespace pldm
