#pragma once

#include "common/utils.hpp"

#include <com/ibm/ipzvpd/Location/server.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <map>
#include <memory>
#include <string>

namespace pldm
{
namespace dbus
{

using OjectPath = std::string;

using LocationIntf = sdbusplus::com::ibm::ipzvpd::server::Location;
using OperationalStatusIntf = sdbusplus::xyz::openbmc_project::State::
    Decorator::server::OperationalStatus;

/** @class CustomDBus
 *  @brief This is a custom D-Bus object, used to add D-Bus interface and update
 *         the corresponding properties value.
 */
class CustomDBus
{
  private:
    CustomDBus()
    {}

  public:
    CustomDBus(const CustomDBus&) = delete;
    CustomDBus(CustomDBus&&) = delete;
    CustomDBus& operator=(const CustomDBus&) = delete;
    CustomDBus& operator=(CustomDBus&&) = delete;
    ~CustomDBus() = default;

    static CustomDBus& getCustomDBus()
    {
        static CustomDBus customDBus;
        return customDBus;
    }

  public:
    /** @brief Update the LocationCode property
     *  @param[in] path  - The object path
     *  @param[in] value - The value of the LocationCode property
     */
    void updateLocation(const std::string& path, std::string value);

    /** @brief Update the Functional property
     *  @param[in] path  - The object path
     *  @param[in] value - The value of the Functional property
     */
    void updateOperationalStatus(const std::string& path, bool value);

  private:
    std::map<OjectPath, std::unique_ptr<LocationIntf>> location;

    std::map<OjectPath, std::unique_ptr<OperationalStatusIntf>>
        operationalStatus;
};

} // namespace dbus
} // namespace pldm