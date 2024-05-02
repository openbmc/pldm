#pragma once

#include "common/utils.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

#include <string>
#include <tuple>
#include <vector>

namespace pldm
{
namespace dbus
{
using AssociationsIntf =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using AssociationsObj =
    std::vector<std::tuple<std::string, std::string, std::string>>;

/**
 * @class Associations
 * @brief Represents associations between objects in the DBus
 */
class Associations : public AssociationsIntf
{
  public:
    Associations() = delete;
    ~Associations() = default;
    Associations(const Associations&) = delete;
    Associations& operator=(const Associations&) = delete;
    Associations(Associations&&) = default;
    Associations& operator=(Associations&&) = default;

    Associations(sdbusplus::bus::bus& bus, const std::string& objPath,
                 const std::map<std::string, PropertiesVariant>& vals) :
        AssociationsIntf(bus, objPath.c_str(), vals),
        path(objPath)
    {}

    /** Get value of Associations */
    AssociationsObj associations() const override;

    /** Set value of Associations */
    AssociationsObj associations(AssociationsObj value) override;

  private:
    std::string path;
};

} // namespace dbus
} // namespace pldm
