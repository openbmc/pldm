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

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
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
 *  @details Each decorator interface is created only when the first property
 *           that belongs to it is set, so that termini without FRU support, or
 *           whose FRU data does not map to an interface, do not expose empty
 *           decorator interfaces on D-Bus. If creating an interface fails, the
 *           interfaces created so far are removed and no further interface is
 *           created.
 */
class PldmFruDecorators
{
  public:
    PldmFruDecorators() = delete;
    PldmFruDecorators(const PldmFruDecorators&) = delete;
    PldmFruDecorators& operator=(const PldmFruDecorators&) = delete;
    PldmFruDecorators(PldmFruDecorators&&) = delete;
    PldmFruDecorators& operator=(PldmFruDecorators&&) = delete;
    ~PldmFruDecorators() = default;

    /** @brief Constructor, no interface is put onto the bus yet.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    PldmFruDecorators(sdbusplus::bus_t& bus, const std::string& path) :
        bus(bus), path(path)
    {}

    /** @brief Set value of partNumber in Decorator.Asset */
    void partNumber(std::string value)
    {
        if (auto* intf = ensure(assetIntf, "Decorator.Asset"))
        {
            intf->partNumber(std::move(value));
        }
    }

    /** @brief Set value of serialNumber in Decorator.Asset */
    void serialNumber(std::string value)
    {
        if (auto* intf = ensure(assetIntf, "Decorator.Asset"))
        {
            intf->serialNumber(std::move(value));
        }
    }

    /** @brief Set value of manufacturer in Decorator.Asset */
    void manufacturer(std::string value)
    {
        if (auto* intf = ensure(assetIntf, "Decorator.Asset"))
        {
            intf->manufacturer(std::move(value));
        }
    }

    /** @brief Set value of buildDate in Decorator.Asset */
    void buildDate(std::string value)
    {
        if (auto* intf = ensure(assetIntf, "Decorator.Asset"))
        {
            intf->buildDate(std::move(value));
        }
    }

    /** @brief Set value of model in Decorator.Asset */
    void model(std::string value)
    {
        if (auto* intf = ensure(assetIntf, "Decorator.Asset"))
        {
            intf->model(std::move(value));
        }
    }

    /** @brief Set value of subModel in Decorator.Asset */
    void subModel(std::string value)
    {
        if (auto* intf = ensure(assetIntf, "Decorator.Asset"))
        {
            intf->subModel(std::move(value));
        }
    }

    /** @brief Set value of sparePartNumber in Decorator.Asset */
    void sparePartNumber(std::string value)
    {
        if (auto* intf = ensure(assetIntf, "Decorator.Asset"))
        {
            intf->sparePartNumber(std::move(value));
        }
    }

    /** @brief Set value of assetTag in Decorator.AssetTag */
    void assetTag(std::string value)
    {
        if (auto* intf = ensure(assetTagIntf, "Decorator.AssetTag"))
        {
            intf->assetTag(std::move(value));
        }
    }

    /** @brief Set value of version in Decorator.Revision */
    void version(std::string value)
    {
        if (auto* intf = ensure(revisionIntf, "Decorator.Revision"))
        {
            intf->version(std::move(value));
        }
    }

    /** @brief Set value of names in Decorator.Compatible */
    void names(std::vector<std::string> values)
    {
        if (auto* intf = ensure(compatibleIntf, "Decorator.Compatible"))
        {
            intf->names(std::move(values));
        }
    }

    /** @brief Get the Decorator.Asset interface
     *  @return the interface, nullptr if it has not been created
     */
    const AssetIntf* getAsset() const
    {
        return assetIntf.get();
    }

    /** @brief Get the Decorator.AssetTag interface
     *  @return the interface, nullptr if it has not been created
     */
    const AssetTagIntf* getAssetTag() const
    {
        return assetTagIntf.get();
    }

    /** @brief Get the Decorator.Revision interface
     *  @return the interface, nullptr if it has not been created
     */
    const RevisionIntf* getRevision() const
    {
        return revisionIntf.get();
    }

    /** @brief Get the Decorator.Compatible interface
     *  @return the interface, nullptr if it has not been created
     */
    const CompatibleIntf* getCompatible() const
    {
        return compatibleIntf.get();
    }

  private:
    /** @brief Create the interface on first use.
     *
     *  On failure the error is logged once, the interfaces created so far are
     *  removed, and nothing is created afterwards.
     *
     *  @param[in,out] intf - the interface to create
     *  @param[in] name - interface name used in the error log
     *  @return the interface, nullptr if it cannot be created
     */
    template <typename Intf>
    Intf* ensure(std::unique_ptr<Intf>& intf, const char* name)
    {
        if (failed)
        {
            return nullptr;
        }
        if (!intf)
        {
            try
            {
                intf = std::make_unique<Intf>(bus, path.c_str());
            }
            catch (const sdbusplus::exception_t& e)
            {
                lg2::error("Failed to create {INTERFACE} at {PATH}: {ERROR}",
                           "INTERFACE", name, "PATH", path, "ERROR", e);
                failed = true;
                assetIntf.reset();
                assetTagIntf.reset();
                revisionIntf.reset();
                compatibleIntf.reset();
                return nullptr;
            }
        }
        return intf.get();
    }

    sdbusplus::bus_t& bus;
    std::string path;
    bool failed = false;
    std::unique_ptr<AssetIntf> assetIntf = nullptr;
    std::unique_ptr<AssetTagIntf> assetTagIntf = nullptr;
    std::unique_ptr<RevisionIntf> revisionIntf = nullptr;
    std::unique_ptr<CompatibleIntf> compatibleIntf = nullptr;
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
