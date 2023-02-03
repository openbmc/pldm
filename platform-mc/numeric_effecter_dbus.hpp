#pragma once

#include "common/types.hpp"
#include "sensor_intf.hpp"

namespace pldm
{
namespace platform_mc
{
/**
 * @brief NumericEffecterDbus
 *
 * This class handles sensor reading updated by sensor manager and export
 * status to D-Bus interface.
 */
class NumericEffecterDbus : public pldm::platform_mc::SensorIntf
{
  public:
    NumericEffecterDbus(const uint8_t tId, const bool sensorDisabled,
                        const std::vector<uint8_t>& pdrData, uint16_t sensorId,
                        std::string& sensorName, std::string& associationPath);
    ~NumericEffecterDbus(){};

    std::vector<uint8_t> getReadingRequest();
    int handleReadingResponse(const pldm_msg* responseMsg, size_t responseLen);
};
} // namespace platform_mc
} // namespace pldm
