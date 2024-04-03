#pragma once

#include "serialize.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/CpuCore/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{
using CoreIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::CpuCore>;

/** @class CPUCore
 *  @brief This class is mapped to CPUCore properties in D-Bus interface path
 * and update/retrieved the corresponding properties value.
 */
class CPUCore : public CoreIntf
{
  public:
    CPUCore() = delete;
    ~CPUCore() = default;
    CPUCore(const CPUCore&) = delete;
    CPUCore& operator=(const CPUCore&) = delete;
    CPUCore(CPUCore&&) = default;
    CPUCore& operator=(CPUCore&&) = default;

    CPUCore(sdbusplus::bus::bus& bus, const std::string& objPath) :
        CoreIntf(bus, objPath.c_str()), path(objPath)
    {
        pldm::serialize::Serialize::getSerialize().serialize(path, "CPUCore");
    }

    /** Get value of Microcode */
    uint32_t microcode() const override;

    /** Set value of Microcode */
    uint32_t microcode(uint32_t value) override;

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
