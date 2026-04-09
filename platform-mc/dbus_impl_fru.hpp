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

#include <map>
#include <memory>

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

using AssetIntf = sdbusplus::server::object_t<assetserver>;
using AssetTagIntf = sdbusplus::server::object_t<assettagserver>;
using RevisionIntf = sdbusplus::server::object_t<revisionserver>;
using CompatibleIntf = sdbusplus::server::object_t<compatibleserver>;

/** @class PldmEntityBase
 *  @brief Abstract base for PLDM inventory entities.
 *  @details Provides property setters via Decorator interfaces. Concrete
 *           subclasses add the entity-type-specific Inventory.Item interface.
 */
class PldmEntityBase
{
  public:
    PldmEntityBase() = delete;
    PldmEntityBase(const PldmEntityBase&) = delete;
    PldmEntityBase& operator=(const PldmEntityBase&) = delete;
    PldmEntityBase(PldmEntityBase&&) = delete;
    PldmEntityBase& operator=(PldmEntityBase&&) = delete;
    virtual ~PldmEntityBase() = default;

    /** @brief Set value of partNumber in Decorator.Asset */
    virtual std::string partNumber(std::string value) = 0;

    /** @brief Set value of serialNumber in Decorator.Asset */
    virtual std::string serialNumber(std::string value) = 0;

    /** @brief Set value of manufacturer in Decorator.Asset */
    virtual std::string manufacturer(std::string value) = 0;

    /** @brief Set value of buildDate in Decorator.Asset */
    virtual std::string buildDate(std::string value) = 0;

    /** @brief Set value of model in Decorator.Asset */
    virtual std::string model(std::string value) = 0;

    /** @brief Set value of subModel in Decorator.Asset */
    virtual std::string subModel(std::string value) = 0;

    /** @brief Set value of sparePartNumber in Decorator.Asset */
    virtual std::string sparePartNumber(std::string value) = 0;

    /** @brief Set value of assetTag in Decorator.AssetTag */
    virtual std::string assetTag(std::string value) = 0;

    /** @brief Set value of version in Decorator.Revision */
    virtual std::string version(std::string value) = 0;

    /** @brief Set value of names in Decorator.Compatible */
    virtual std::vector<std::string> names(std::vector<std::string> values) = 0;

  protected:
    PldmEntityBase(sdbusplus::bus_t& /*bus*/, const std::string& /*path*/) {}
};

/** @class PldmEntityReq
 *  @brief Templated PLDM inventory entity implementation.
 *  @details Inherits Decorator interfaces for properties and an
 *           entity-type-specific Inventory.Item marker interface.
 *  @tparam ItemServer - The sdbusplus server type for the Item interface
 */
template <typename ItemServer>
class PldmEntityReq :
    public PldmEntityBase,
    public AssetIntf,
    public AssetTagIntf,
    public RevisionIntf,
    public CompatibleIntf,
    public sdbusplus::server::object_t<ItemServer>
{
  public:
    using ItemIntf = sdbusplus::server::object_t<ItemServer>;

    PldmEntityReq() = delete;
    PldmEntityReq(const PldmEntityReq&) = delete;
    PldmEntityReq& operator=(const PldmEntityReq&) = delete;
    PldmEntityReq(PldmEntityReq&&) = delete;
    PldmEntityReq& operator=(PldmEntityReq&&) = delete;
    ~PldmEntityReq() override = default;

    PldmEntityReq(sdbusplus::bus_t& bus, const std::string& path) :
        PldmEntityBase(bus, path), AssetIntf(bus, path.c_str()),
        AssetTagIntf(bus, path.c_str()), RevisionIntf(bus, path.c_str()),
        CompatibleIntf(bus, path.c_str()), ItemIntf(bus, path.c_str())
    {}

    std::string partNumber(std::string value) override
    {
        return AssetIntf::partNumber(std::move(value));
    }
    std::string serialNumber(std::string value) override
    {
        return AssetIntf::serialNumber(std::move(value));
    }
    std::string manufacturer(std::string value) override
    {
        return AssetIntf::manufacturer(std::move(value));
    }
    std::string buildDate(std::string value) override
    {
        return AssetIntf::buildDate(std::move(value));
    }
    std::string model(std::string value) override
    {
        return AssetIntf::model(std::move(value));
    }
    std::string subModel(std::string value) override
    {
        return AssetIntf::subModel(std::move(value));
    }
    std::string sparePartNumber(std::string value) override
    {
        return AssetIntf::sparePartNumber(std::move(value));
    }
    std::string assetTag(std::string value) override
    {
        return AssetTagIntf::assetTag(std::move(value));
    }
    std::string version(std::string value) override
    {
        return RevisionIntf::version(std::move(value));
    }
    std::vector<std::string> names(std::vector<std::string> values) override
    {
        return CompatibleIntf::names(std::move(values));
    }
};

// Item interface server types
using boardserver =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Board;
using chassisserver =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Chassis;
using cpuserver = sdbusplus::xyz::openbmc_project::Inventory::Item::server::Cpu;
using dimmserver =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm;
using fanserver = sdbusplus::xyz::openbmc_project::Inventory::Item::server::Fan;
using powersupplyserver =
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PowerSupply;
using acceleratorserver =
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
            return std::make_unique<PldmEntityReq<chassisserver>>(bus, path);
        case PLDM_ENTITY_PROC:
            return std::make_unique<PldmEntityReq<cpuserver>>(bus, path);
        case PLDM_ENTITY_MEMORY_MODULE:
            return std::make_unique<PldmEntityReq<dimmserver>>(bus, path);
        case PLDM_ENTITY_FAN:
            return std::make_unique<PldmEntityReq<fanserver>>(bus, path);
        case PLDM_ENTITY_POWER_SUPPLY:
            return std::make_unique<PldmEntityReq<powersupplyserver>>(
                bus, path);
        case PLDM_ENTITY_GPU:
        case PLDM_ENTITY_ACCELERATOR:
            return std::make_unique<PldmEntityReq<acceleratorserver>>(
                bus, path);
        case PLDM_ENTITY_BOARD:
        case PLDM_ENTITY_SYS_BOARD:
        case PLDM_ENTITY_CARD:
        default:
            return std::make_unique<PldmEntityReq<boardserver>>(bus, path);
    }
}

} // namespace dbus_api
} // namespace pldm
