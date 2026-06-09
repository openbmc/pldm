#pragma once

#include "../../libpldmresponder/platform.hpp"
#include "../../oem/arm/event/oem_event_manager.hpp"

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

    explicit OemARM(responder::platform::Handler* platformHandler,
                    platform_mc::Manager* platformManager) :
        oemEventManager(std::make_unique<oem_arm::OemEventManager>(
            platformManager))
    {
        registerOemEventHandlers(platformHandler);
    }

  private:
    void registerOemEventHandlers(
        responder::platform::Handler* platformHandler)
    {
        auto* eventManager = oemEventManager.get();

        platformHandler->registerEventHandlers(
            PLDM_SENSOR_EVENT,
            {[eventManager](const pldm_msg* request, size_t payloadLength,
                            uint8_t formatVersion, uint8_t tid,
                            size_t eventDataOffset) {
                return eventManager->handleSensorEvent(
                    request, payloadLength, formatVersion, tid,
                    eventDataOffset);
            }});
    }

    std::unique_ptr<oem_arm::OemEventManager> oemEventManager{};
};

} // namespace oem_arm
} // namespace pldm
