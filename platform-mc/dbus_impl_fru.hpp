#pragma once

#include "xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/AssetTag/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/Compatible/server.hpp"
#include "xyz/openbmc_project/Inventory/Decorator/Revision/server.hpp"
#include "xyz/openbmc_project/Inventory/Item/Accelerator/server.hpp"
#include "xyz/openbmc_project/Inventory/Item/Board/server.hpp"
#include "xyz/openbmc_project/Inventory/Item/Chassis/server.hpp"
#include "xyz/openbmc_project/Inventory/Item/Cpu/server.hpp"
#include "xyz/openbmc_project/Inventory/Item/Dimm/server.hpp"
#include "xyz/openbmc_project/Inventory/Item/Fan/server.hpp"
#include "xyz/openbmc_project/Inventory/Item/PowerSupply/server.hpp"

#include <libpldm/entity.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>

#include <memory>
#include <string>
#include <vector>

namespace pldm
{
namespace dbus_api
{

using AssetServer =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset;
using AssetTagServer =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::AssetTag;
using RevisionServer =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Revision;
using CompatibleServer =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Compatible;

using AssetIntf = sdbusplus::server::object_t<AssetServer>;
using AssetTagIntf = sdbusplus::server::object_t<AssetTagServer>;
using RevisionIntf = sdbusplus::server::object_t<RevisionServer>;
using CompatibleIntf = sdbusplus::server::object_t<CompatibleServer>;

/** @class PldmEntityBase
 *  @brief Abstract base for PLDM inventory entities.
 *  @details Provides a common base type for the entity-type-specific
 *           PldmEntityReq<T> template instantiations, allowing the
 *           Terminus to hold any entity type through a single pointer.
 */
class PldmEntityBase
{
  public:
    PldmEntityBase() = delete;
    PldmEntityBase(const PldmEntityBase&) = delete;
    PldmEntityBase& operator=(const PldmEntityBase&) = delete;
    PldmEntityBase(PldmEntityBase&&) noexcept = default;
    PldmEntityBase& operator=(PldmEntityBase&&) noexcept = default;
    virtual ~PldmEntityBase() = default;

  protected:
    PldmEntityBase(sdbusplus::bus_t& /*bus*/, const std::string& /*path*/) {}
};

/** @class PldmEntityReq
 *  @brief Templated PLDM inventory entity implementation.
 *  @details Inherits an entity-type-specific Inventory.Item marker interface.
 *           Decorator interfaces are handled separately by PldmFruDecorators,
 *           which is created lazily only when FRU record data is available.
 *  @tparam ItemServer - The sdbusplus server type for the Item interface
 */
template <typename ItemServer>
class PldmEntityReq :
    public PldmEntityBase,
    public sdbusplus::server::object_t<ItemServer>
{
  public:
    using ItemIntf = sdbusplus::server::object_t<ItemServer>;

    PldmEntityReq() = delete;
    PldmEntityReq(const PldmEntityReq&) = delete;
    PldmEntityReq& operator=(const PldmEntityReq&) = delete;
    PldmEntityReq(PldmEntityReq&&) noexcept = default;
    PldmEntityReq& operator=(PldmEntityReq&&) noexcept = default;
    ~PldmEntityReq() override = default;

    PldmEntityReq(sdbusplus::bus_t& bus, const std::string& path) :
        PldmEntityBase(bus, path), ItemIntf(bus, path.c_str())
    {}
};

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
        RevisionIntf(bus, path.c_str()), CompatibleIntf(bus, path.c_str())
    {}

    /** @brief Set value of partNumber in Decorator.Asset */
    std::string partNumber(std::string value) override
    {
        return AssetIntf::partNumber(std::move(value));
    }

    /** @brief Set value of serialNumber in Decorator.Asset */
    std::string serialNumber(std::string value) override
    {
        return AssetIntf::serialNumber(std::move(value));
    }

    /** @brief Set value of manufacturer in Decorator.Asset */
    std::string manufacturer(std::string value) override
    {
        return AssetIntf::manufacturer(std::move(value));
    }

    /** @brief Set value of buildDate in Decorator.Asset */
    std::string buildDate(std::string value) override
    {
        return AssetIntf::buildDate(std::move(value));
    }

    /** @brief Set value of model in Decorator.Asset */
    std::string model(std::string value) override
    {
        return AssetIntf::model(std::move(value));
    }

    /** @brief Set value of subModel in Decorator.Asset */
    std::string subModel(std::string value) override
    {
        return AssetIntf::subModel(std::move(value));
    }

    /** @brief Set value of sparePartNumber in Decorator.Asset */
    std::string sparePartNumber(std::string value) override
    {
        return AssetIntf::sparePartNumber(std::move(value));
    }

    /** @brief Set value of assetTag in Decorator.AssetTag */
    std::string assetTag(std::string value) override
    {
        return AssetTagIntf::assetTag(std::move(value));
    }

    /** @brief Set value of version in Decorator.Revision */
    std::string version(std::string value) override
    {
        return RevisionIntf::version(std::move(value));
    }

    /** @brief Set value of names in Decorator.Compatible */
    std::vector<std::string> names(std::vector<std::string> values) override
    {
        return CompatibleIntf::names(std::move(values));
    }
};

// Item interface server types
using BoardServer =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Board;
using ChassisServer =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis;
using CpuServer = sdbusplus::xyz::openbmc_project::Inventory::Item::server::Cpu;
using DimmServer =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm;
using FanServer = sdbusplus::xyz::openbmc_project::Inventory::Item::server::Fan;
using PowerSupplyServer =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PowerSupply;
using AcceleratorServer =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Accelerator;

/** @brief Create the appropriate PldmEntityReq for the given entity type.
 *  @param[in] bus - D-Bus bus
 *  @param[in] path - D-Bus object path
 *  @param[in] entityType - PLDM entity type
 *  @return unique_ptr to PldmEntityBase
 */
inline std::unique_ptr<PldmEntityBase> createPldmEntity(
    sdbusplus::bus_t& bus, const std::string& path, uint16_t entityType)
{
    switch (entityType)
    {
        case PLDM_ENTITY_SYSTEM_CHASSIS:
            return std::make_unique<PldmEntityReq<ChassisServer>>(bus, path);
        case PLDM_ENTITY_PROC:
            return std::make_unique<PldmEntityReq<CpuServer>>(bus, path);
        case PLDM_ENTITY_MEMORY_MODULE:
            return std::make_unique<PldmEntityReq<DimmServer>>(bus, path);
        case PLDM_ENTITY_FAN:
            return std::make_unique<PldmEntityReq<FanServer>>(bus, path);
        case PLDM_ENTITY_POWER_SUPPLY:
            return std::make_unique<PldmEntityReq<PowerSupplyServer>>(
                bus, path);
        case PLDM_ENTITY_GPU:
        case PLDM_ENTITY_ACCELERATOR:
            return std::make_unique<PldmEntityReq<AcceleratorServer>>(
                bus, path);
        case PLDM_ENTITY_BOARD:
        case PLDM_ENTITY_SYS_BOARD:
        case PLDM_ENTITY_CARD:
        default:
            return std::make_unique<PldmEntityReq<BoardServer>>(bus, path);
    }
}

} // namespace dbus_api
} // namespace pldm
