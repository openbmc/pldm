#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/Cable/server.hpp>

#include <string>

namespace pldm
{
namespace dbus
{

using ItemCable = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Cable>;

/**
 * @class Cable
 * @brief Dbus support for cable interface and attributes
 */
class Cable : public ItemCable
{
  public:
    Cable() = delete;
    ~Cable() = default;
    Cable(const Cable&) = delete;
    Cable& operator=(const Cable&) = delete;
    Cable(Cable&&) = default;
    Cable& operator=(Cable&&) = default;

    Cable(sdbusplus::bus::bus& bus, const std::string& objPath) :
        ItemCable(bus, objPath.c_str()), path(objPath)
    {
        // cable objects does not need to be store in serialized memory
    }

    /** Get length of the cable in meters */
    double length() const override;

    /** Set length of the cable in meters */
    double length(double value) override;

    /** Get string used to provide the type of
          a cable, such as optical or coppervalue */
    std::string cableTypeDescription() const override;

    /** Set Cable type description */
    std::string cableTypeDescription(std::string value) override;

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
