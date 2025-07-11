#pragma once

#include "libpldmresponder/platform.hpp"
#include "oem/meta/event/oem_event_manager.hpp"

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
    OemMETA() = default;
    OemMETA(const OemMETA&) = delete;
    OemMETA& operator=(const OemMETA&) = delete;
    OemMETA(OemMETA&&) = delete;
    OemMETA& operator=(OemMETA&&) = delete;

  public:
    /** @brief Constucts OemMETA object
     *  @param[in] platformHandler - platformHandler handler
     */
    explicit OemMETA(pldm::responder::platform::Handler* platformHandler);

  private:
    void registerOemEventHandler(
        pldm::responder::platform::Handler* platformHandler);

    std::unique_ptr<oem_meta::OemEventManager> oemEventManager{};
};

} // namespace pldm::oem_meta
