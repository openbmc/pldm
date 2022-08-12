#pragma once

#include "common/types.hpp"
#include "sensor_intf.hpp"

namespace pldm
{
namespace platform_mc
{

/**
 * @brief CompactNumericSensor
 *
 * This class handles sensor reading updated by sensor manager and export
 * status to D-Bus interface.
 */
class CompactNumericSensor : public pldm::platform_mc::SensorIntf
{
  public:
    CompactNumericSensor(const tid_t tId, const bool sensorDisabled,
                         const std::vector<uint8_t>& pdrData, uint16_t sensorId,
                         std::string& sensorName, std::string& associationPath);
    ~CompactNumericSensor(){};
};
} // namespace platform_mc
} // namespace pldm
