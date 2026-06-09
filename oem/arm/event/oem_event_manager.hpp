#pragma once

#include "common/types.hpp"

#include <libpldm/base.h>

#include <cstddef>
#include <cstdint>
#include <string>

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

    explicit OemEventManager(platform_mc::Manager* manager) : manager(manager) {}

    int handleSensorEvent(const pldm_msg* request, size_t payloadLength,
                          uint8_t formatVersion, pldm_tid_t tid,
                          size_t eventDataOffset);

    int handlePolledSensorEvent(pldm_tid_t tid, uint16_t eventId,
                                const uint8_t* eventData,
                                size_t eventDataSize);

  private:
    int processNumericSensorEvent(pldm_tid_t tid, uint16_t sensorId,
                                  const uint8_t* sensorData,
                                  size_t sensorDataLength);
    bool isBootProgressCodeSensor(pldm_tid_t tid, uint16_t sensorId) const;
    int updateBootRawFromBootProgress(uint32_t presentReading) const;

    platform_mc::Manager* manager = nullptr;
};

} // namespace oem_arm
} // namespace pldm
