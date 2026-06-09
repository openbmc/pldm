#pragma once

#include "../../libpldmresponder/platform.hpp"
#include "../../oem/arm/event/oem_event_manager.hpp"

#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>

#include <memory>

namespace pldm
{
namespace platform_mc
{
class Manager;
}

namespace oem_arm
{

/**
 * @class OemARM
 *
 * @brief Registers Arm-specific PLDM handlers.
 */
class OemARM
{
  public:
    OemARM() = delete;
    OemARM(const OemARM&) = delete;
    OemARM(OemARM&&) = delete;
    OemARM& operator=(const OemARM&) = delete;
    OemARM& operator=(OemARM&&) = delete;

    /** @brief Construct the Arm OEM handler registration object
     *
     *  @param[in] platformHandler - Platform responder handler
     *  @param[in] platformManager - Platform manager used by event handlers
     */
    explicit OemARM(responder::platform::Handler* platformHandler,
                    platform_mc::Manager* platformManager) :
        oemEventManager(
            std::make_unique<oem_arm::OemEventManager>(platformManager))
    {
        registerOemEventHandlers(platformHandler);
    }

  private:
    /** @brief Register Arm OEM event handlers
     *
     *  @param[in] platformHandler - Platform responder handler
     */
    void registerOemEventHandlers(responder::platform::Handler* platformHandler)
    {
        if (!platformHandler)
        {
            lg2::error("Unable to register Arm OEM event handlers; platform "
                       "handler is null");
            return;
        }

        auto* eventManager = oemEventManager.get();
        if (!eventManager)
        {
            lg2::error("Unable to register Arm OEM event handlers; event "
                       "manager is null");
            return;
        }

        platformHandler->registerEventHandlers(
            PLDM_SENSOR_EVENT,
            {[eventManager](const pldm_msg* request, size_t payloadLength,
                            uint8_t formatVersion, uint8_t tid,
                            size_t eventDataOffset) {
                if (!eventManager)
                {
                    lg2::error("Unable to handle Arm OEM sensor event; event "
                               "manager is null");
                    return static_cast<int>(PLDM_ERROR);
                }

                return eventManager->handleSensorEvent(
                    request, payloadLength, formatVersion, tid,
                    eventDataOffset);
            }});
    }

    std::unique_ptr<oem_arm::OemEventManager> oemEventManager{};
};

} // namespace oem_arm
} // namespace pldm
