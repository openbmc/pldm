#pragma once

#include "../../libpldmresponder/platform.hpp"
#include "../../oem/arm/event/oem_event_manager.hpp"
#include "../../platform-mc/manager.hpp"

#include <memory>

namespace pldm
{
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
                    platform_mc::Manager* platformManager)
    {
        oemEventManager =
            std::make_shared<oem_arm::OemEventManager>(platformManager);
        createOemEventHandler(oemEventManager.get(), platformHandler,
                              platformManager);
    }

  private:
    void createOemEventHandler(oem_arm::OemEventManager* oemEventManager,
                               responder::platform::Handler* platformHandler,
                               platform_mc::Manager* platformManager)
    {
        platformHandler->registerEventHandlers(
            PLDM_SENSOR_EVENT,
            {[oemEventManager](const pldm_msg* request, size_t payloadLength,
                               uint8_t formatVersion, uint8_t tid,
                               size_t eventDataOffset) {
                return oemEventManager->handleSensorEvent(
                    request, payloadLength, formatVersion, tid,
                    eventDataOffset);
            }});

        platformManager->registerPolledEventHandler(
            PLDM_SENSOR_EVENT,
            {[oemEventManager](pldm_tid_t tid, uint16_t eventId,
                               const uint8_t* eventData, size_t eventDataSize) {
                return oemEventManager->handlePolledSensorEvent(
                    tid, eventId, eventData, eventDataSize);
            }});
    }

    std::shared_ptr<oem_arm::OemEventManager> oemEventManager{};
};

} // namespace oem_arm
} // namespace pldm
