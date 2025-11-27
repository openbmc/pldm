#pragma once

#include "libpldmresponder/platform.hpp"
#include "platform-mc/manager.hpp"

namespace pldm
{
namespace oem_nvidia
{

static constexpr const uint8_t PLDM_OEM_NVIDIA_LEGACY_CPER_EVENT_CLASS = 0xFA;

/**
 * @class OemNVIDIA
 *
 * @brief class for creating all the OEM NVIDIA handlers
 */
class OemNVIDIA
{
  public:
    OemNVIDIA() = delete;
    OemNVIDIA& operator=(const OemNVIDIA&) = delete;
    OemNVIDIA(OemNVIDIA&&) = delete;
    OemNVIDIA& operator=(OemNVIDIA&&) = delete;

  public:
    /** Constructs OemNVIDIA object
     *
     * @param[in] platformHandler - platformHandler handler
     * @param[in] platformManager - Platform Manager
     */
    explicit OemNVIDIA(responder::platform::Handler* platformHandler,
                       platform_mc::Manager* platformManager)
    {
        createOemEventHandler(platformHandler, platformManager);
    }

  private:
    /** @brief Method for creating OEM event handlers
     *
     *  This method registers handlers for backwards compatibility with
     *  event class 0xFA (legacy CPER event class before DSP0248 V1.3.0)
     */
    void createOemEventHandler(responder::platform::Handler* platformHandler,
                               platform_mc::Manager* platformManager)
    {
        /** CPEREvent class (0x07) is only available in DSP0248 V1.3.0.
         *  Before DSP0248 V1.3.0 spec, NVIDIA used OEM event class 0xFA to
         *  report CPER events. For backwards compatibility, handle both
         *  0xFA and standard PLDM_CPER_EVENT (0x07).
         */
        platformHandler->registerEventHandlers(
            PLDM_OEM_NVIDIA_LEGACY_CPER_EVENT_CLASS,
            {[platformManager](const pldm_msg* request, size_t payloadLength,
                               uint8_t formatVersion, uint8_t tid,
                               size_t eventDataOffset) {
                return platformManager->handleCperEvent(
                    request, payloadLength, formatVersion, tid,
                    eventDataOffset);
            }});

        /* Support handling polled events with NVIDIA OEM CPER event class 0xFA
         */
        platformManager->registerPolledEventHandler(
            PLDM_OEM_NVIDIA_LEGACY_CPER_EVENT_CLASS,
            {[platformManager](pldm_tid_t tid, uint16_t eventId,
                               const uint8_t* eventData, size_t eventDataSize) {
                return platformManager->handlePolledCperEvent(
                    tid, eventId, eventData, eventDataSize);
            }});
    }
};

} // namespace oem_nvidia
} // namespace pldm
