#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeSlot/common.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using ItemDevice = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PCIeDevice>;
using Generations = sdbusplus::common::xyz::openbmc_project::inventory::item::
    PCIeSlot::Generations;
// sdbusplus::xyz::openbmc_project::Inventory::Item::PCIeSlot::Generations;

/**
 * @class PCIeDevice
 * @brief PCIeDevice DBUS support, also includes the device properties
 */

class PCIeDevice : public ItemDevice
{
  public:
    PCIeDevice() = delete;
    ~PCIeDevice() = default;
    PCIeDevice(const PCIeDevice&) = delete;
    PCIeDevice& operator=(const PCIeDevice&) = delete;
    PCIeDevice(PCIeDevice&&) = default;
    PCIeDevice& operator=(PCIeDevice&&) = default;

    PCIeDevice(sdbusplus::bus::bus& bus, const std::string& objPath) :
        ItemDevice(bus, objPath.c_str()), path(objPath)
    {}

    /** Get lanes in use */
    size_t lanesInUse() const override;

    /** Set lanes in use */
    size_t lanesInUse(size_t value) override;

    /** Get Generation in use */
    Generations generationInUse() const override;

    /** Set Generation in use */
    Generations generationInUse(Generations value) override;

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
