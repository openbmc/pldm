#pragma once

#include <libpldm/base.h>

#include <cstddef>
#include <cstdint>

namespace pldm
{
namespace platform_mc
{
class Manager;
}

namespace oem_arm
{

class OemEventManager
{
  public:
    OemEventManager() = delete;
    OemEventManager(const OemEventManager&) = delete;
    OemEventManager(OemEventManager&&) = delete;
    OemEventManager& operator=(const OemEventManager&) = delete;
    OemEventManager& operator=(OemEventManager&&) = delete;
    ~OemEventManager() = default;

    /** @brief Construct the Arm OEM event manager
     *
     *  @param[in] manager - Platform manager used to resolve terminus names
     */
    explicit OemEventManager(platform_mc::Manager* manager) : manager(manager)
    {}

    /** @brief Handle an Arm OEM sensor event
     *
     *  @param[in] request - PLDM request message
     *  @param[in] payloadLength - Request payload length
     *  @param[in] formatVersion - Event message format version
     *  @param[in] tid - Terminus ID that sent the event
     *  @param[in] eventDataOffset - Offset of the event data in the payload
     *
     *  @return PLDM completion code
     */
    int handleSensorEvent(const pldm_msg* request, size_t payloadLength,
                          uint8_t formatVersion, pldm_tid_t tid,
                          size_t eventDataOffset);

  private:
    /** @brief Check whether a terminus is the PHX terminus
     *
     *  @param[in] tid - Terminus ID
     *
     *  @return true if the terminus name is PHX, false otherwise
     */
    bool isPhxTerminus(pldm_tid_t tid) const;

    /** @brief Decode a PLDM sensor event payload
     *
     *  @param[in] tid - Terminus ID that sent the event
     *  @param[in] eventData - Sensor event data
     *  @param[in] eventDataSize - Sensor event data size
     *
     *  @return PLDM completion code
     */
    int decodeSensorEvent(pldm_tid_t tid, const uint8_t* eventData,
                          size_t eventDataSize);

    /** @brief Process numeric sensor event data
     *
     *  @param[in] tid - Terminus ID that sent the event
     *  @param[in] sensorId - Sensor ID from the event
     *  @param[in] sensorData - Numeric sensor event data
     *  @param[in] sensorDataLength - Numeric sensor event data length
     *
     *  @return PLDM completion code
     */
    int processNumericSensorEvent(pldm_tid_t tid, uint16_t sensorId,
                                  const uint8_t* sensorData,
                                  size_t sensorDataLength);

    /** @brief Decode and route Arm OEM state sensor events.
     *
     * Device File state sensor events are routed to the crashlog collector.
     *
     * @param[in] tid Terminus ID that emitted the event.
     * @param[in] sensorId State sensor ID.
     * @param[in] sensorData State sensor payload bytes.
     * @param[in] sensorDataLength Number of payload bytes.
     * @return PLDM completion code.
     */
    int processStateSensorEvent(pldm_tid_t tid, uint16_t sensorId,
                                const uint8_t* sensorData,
                                size_t sensorDataLength);

    /** @brief Update boot progress D-Bus properties
     *
     *  @param[in] presentReading - Boot progress reading from the sensor event
     *
     *  @return PLDM completion code
     */
    int updateBootProgress(uint32_t presentReading) const;

    platform_mc::Manager* manager = nullptr;
};

} // namespace oem_arm
} // namespace pldm
