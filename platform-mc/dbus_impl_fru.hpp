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

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>

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
 *  @details Provides property setters via Decorator interfaces. Concrete
 *           subclasses add the entity-type-specific Inventory.Item interface.
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
    PldmEntityReq(PldmEntityReq&&) noexcept = default;
    PldmEntityReq& operator=(PldmEntityReq&&) noexcept = default;
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

/** @brief Create the PldmEntityReq of the ItemServer interface
 *  @param[in] bus - D-Bus bus
 *  @param[in] path - D-Bus object path
 *  @return unique_ptr to PldmEntityBase
 */
template <typename ItemServer>
std::unique_ptr<PldmEntityBase> makePldmEntity(sdbusplus::bus_t& bus,
                                               const std::string& path)
{
    return std::make_unique<PldmEntityReq<ItemServer>>(bus, path);
}

using PldmEntityCreator =
    std::unique_ptr<PldmEntityBase> (*)(sdbusplus::bus_t&, const std::string&);

/** @struct PldmEntityItem
 *  @brief The name and the Inventory.Item interface of one PLDM entity type.
 */
struct PldmEntityItem
{
    uint16_t entityType;      //!< PLDM entity type
    std::string_view name;    //!< Name of the entity type
    PldmEntityCreator create; //!< Creator of the Inventory.Item interface
    //! The entity type may be contained directly by the terminus
    bool containedByTerminus;
};

/** @brief The PLDM entity types which have an Inventory.Item interface.
 *
 *  The name identifies the entity type in a D-Bus object name, so it is
 *  unique per entity type even where two entity types share an
 *  Inventory.Item interface.
 *
 *  containedByTerminus marks the entity types which the terminus inventory
 *  object may contain directly, so that an entity of the type is reachable
 *  from the terminus when the container of the entity has no D-Bus object.
 */
inline constexpr std::array<PldmEntityItem, 10> pldmEntityItems{{
    {PLDM_ENTITY_SYSTEM_CHASSIS, "Chassis", makePldmEntity<ChassisServer>,
     false},
    {PLDM_ENTITY_PROC, "Cpu", makePldmEntity<CpuServer>, true},
    {PLDM_ENTITY_MEMORY_MODULE, "Dimm", makePldmEntity<DimmServer>, false},
    {PLDM_ENTITY_FAN, "Fan", makePldmEntity<FanServer>, false},
    {PLDM_ENTITY_POWER_SUPPLY, "PowerSupply", makePldmEntity<PowerSupplyServer>,
     false},
    {PLDM_ENTITY_GPU, "Gpu", makePldmEntity<AcceleratorServer>, true},
    {PLDM_ENTITY_ACCELERATOR, "Accelerator", makePldmEntity<AcceleratorServer>,
     true},
    {PLDM_ENTITY_BOARD, "Board", makePldmEntity<BoardServer>, false},
    {PLDM_ENTITY_SYS_BOARD, "SysBoard", makePldmEntity<BoardServer>, false},
    {PLDM_ENTITY_CARD, "Card", makePldmEntity<BoardServer>, false},
}};

/** @brief Find the entry of the given entity type
 *  @param[in] entityType - PLDM entity type
 *  @return pointer to the entry, nullptr when the entity type has no
 *          Inventory.Item interface
 */
inline const PldmEntityItem* findPldmEntityItem(uint16_t entityType)
{
    auto it = std::ranges::find(pldmEntityItems, entityType,
                                &PldmEntityItem::entityType);
    if (it == pldmEntityItems.end())
    {
        return nullptr;
    }
    return &*it;
}

/** @brief Get the name of the given entity type
 *  @param[in] entityType - PLDM entity type
 *  @return the entity type name, nullopt when the entity type has no
 *          Inventory.Item interface
 */
inline std::optional<std::string_view> getPldmEntityName(uint16_t entityType)
{
    auto item = findPldmEntityItem(entityType);
    if (!item)
    {
        return std::nullopt;
    }
    return item->name;
}

/** @brief Check whether the terminus may contain the given entity type
 *  directly
 *  @param[in] entityType - PLDM entity type
 *  @return true when the terminus inventory object may contain the entity type
 */
inline bool isPldmEntityContainedByTerminus(uint16_t entityType)
{
    auto item = findPldmEntityItem(entityType);
    return item && item->containedByTerminus;
}

/** @brief Create the PldmEntityReq which matches the given entity type.
 *  @param[in] bus - D-Bus bus
 *  @param[in] path - D-Bus object path
 *  @param[in] entityType - PLDM entity type
 *  @return unique_ptr to PldmEntityBase, nullptr when the entity type has no
 *          matching Inventory.Item interface
 */
inline std::unique_ptr<PldmEntityBase> createPldmEntityForType(
    sdbusplus::bus_t& bus, const std::string& path, uint16_t entityType)
{
    auto item = findPldmEntityItem(entityType);
    if (!item)
    {
        return nullptr;
    }
    return item->create(bus, path);
}

/** @brief Create the appropriate PldmEntityReq for the given entity type.
 *  @param[in] bus - D-Bus bus
 *  @param[in] path - D-Bus object path
 *  @param[in] entityType - PLDM entity type
 *  @return unique_ptr to PldmEntityBase
 */
inline std::unique_ptr<PldmEntityBase> createPldmEntity(
    sdbusplus::bus_t& bus, const std::string& path, uint16_t entityType)
{
    auto entity = createPldmEntityForType(bus, path, entityType);
    if (entity)
    {
        return entity;
    }
    return std::make_unique<PldmEntityReq<BoardServer>>(bus, path);
}

} // namespace dbus_api
} // namespace pldm
