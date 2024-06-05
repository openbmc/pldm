#pragma once

#include "xyz/openbmc_project/Inventory/Source/PLDM/FRU/server.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>

#include <map>

namespace pldm
{
namespace dbus_api
{

using fruserver =
    sdbusplus::xyz::openbmc_project::Inventory::Source::PLDM::server::FRU;

using FruIntf = sdbusplus::server::object::object<fruserver>;

/** @class FruRequester
 *  @brief OpenBMC PLDM.FRU implementation.
 *  @details A concrete implementation for the
 *  xyz.openbmc_project.Inventory.Source.PLDM.FRU DBus APIs.
 */
class FruReq : public FruIntf
{
  public:
    FruReq() = delete;
    FruReq(const FruReq&) = delete;
    FruReq& operator=(const FruReq&) = delete;
    FruReq(FruReq&&) = delete;
    FruReq& operator=(FruReq&&) = delete;
    virtual ~FruReq() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    FruReq(sdbusplus::bus::bus& bus, const std::string& path) :
        FruIntf(bus, path.c_str()){};

    /** @brief Set value of chassisType */
    std::string chassisType(std::string value);

    /** @brief Set value of Model */
    std::string model(std::string value);

    /** @brief Set value of Product Number */
    std::string pn(std::string value);

    /** @brief Set value of Serial Numbre */
    std::string sn(std::string value);

    /** @brief Set value of Manufacture */
    std::string manufacturer(std::string value);

    /** @brief Set value of Manufacture Date */
    std::string manufacturerDate(std::string value);

    /** @brief Set value of Vendor */
    std::string vendor(std::string value);

    /** @brief Set value of Name */
    std::string name(std::string value);

    /** @brief Set value of SKU */
    std::string sku(std::string value);

    /** @brief Set value of Version */
    std::string version(std::string value);

    /** @brief Set value of asset Tag */
    std::string assetTag(std::string value);

    /** @brief Set value of Description */
    std::string description(std::string value);

    /** @brief Set value of EC Level */
    std::string ecLevel(std::string value);

    /** @brief Set value of Other */
    std::string other(std::string value);

    /** @brief Set value of IANA */
    uint32_t iana(uint32_t value);

  private:
};

} // namespace dbus_api
} // namespace pldm
