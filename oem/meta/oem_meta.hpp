#pragma once

#include "common/utils.hpp"
#include "utils.hpp"

namespace pldm::oem_meta
{

/**
 * @class OemMETA
 *
 * @brief class for creating all the OEM META handlers. Only in case
 *        of OEM_META this class object will be instantiated
 */

class OemMETA
{
  public:
    OemMETA() = delete;
    OemMETA(const OemMETA&) = delete;
    OemMETA& operator=(const OemMETA&) = delete;
    OemMETA(OemMETA&&) = delete;
    OemMETA& operator=(OemMETA&&) = delete;

  public:
    /** @brief Constucts OemMETA object
     *  @param[in] dBusIntf - D-Bus handler
     */
    explicit OemMETA(const pldm::utils::DBusHandler* dbusHandler);

  private:
    /** @brief D-Bus handler
     */
    const pldm::utils::DBusHandler* dbusHandler;
};

} // namespace pldm::oem_meta
