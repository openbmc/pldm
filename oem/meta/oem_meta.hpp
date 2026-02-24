#pragma once

#include "libpldmresponder/platform.hpp"
#include "oem/meta/event/oem_event_manager.hpp"
#include "oem/meta/libpldmresponder/file_io.hpp"
#include "pldmd/invoker.hpp"

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
    /** @brief Constructs OemMETA object
     *  @param[in] dBusIntf - D-Bus handler
     *  @param[in] invoker - invoker handler
     *  @param[in] platformHandler - platformHandler handler
     */
    explicit OemMETA(const pldm::utils::DBusHandler* dbusHandler,
                     pldm::responder::Invoker& invoker,
                     pldm::responder::platform::Handler* platformHandler);

  private:
    void registerOemEventHandler(
        pldm::responder::platform::Handler* platformHandler);

    void registerOemHandler(
        pldm::responder::Invoker& invoker,
        std::unique_ptr<pldm::responder::oem_meta::FileIOHandler>
            fileIOHandler);

    std::unique_ptr<oem_meta::OemEventManager> oemEventManager{};
};

} // namespace pldm::oem_meta
