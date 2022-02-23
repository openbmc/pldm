#pragma once

#include "common/types.hpp"
#include "sensor_intf.hpp"

namespace pldm
{
namespace platform_mc
{

/**
 * @brief NumericSensor
 *
 * This class handles sensor reading updated by sensor manager and export
 * status to D-Bus interface.
 */
class NumericSensor : public pldm::platform_mc::SensorIntf
{
  public:
    NumericSensor(const tid_t tId, const bool sensorDisabled,
                  const std::vector<uint8_t>& pdrData, uint16_t sensorId,
                  std::string& sensorName, std::string& associationPath);
    ~NumericSensor(){};
};
} // namespace platform_mc
} // namespace pldm
