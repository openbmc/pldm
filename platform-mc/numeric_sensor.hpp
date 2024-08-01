#pragma once

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "common/types.hpp"

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <string>

namespace pldm
{
namespace platform_mc
{

using SensorUnit = sdbusplus::xyz::openbmc_project::Sensor::server::Value::Unit;
using ValueIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::server::Value>;
using ThresholdWarningIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Warning>;
using ThresholdCriticalIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Critical>;
using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;
using AvailabilityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Availability>;
using AssociationDefinitionsInft = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

/**
 * @brief NumericSensor
 *
 * This class handles sensor reading updated by sensor manager and export
 * status to D-Bus interface.
 */
class NumericSensor
{
  public:
    NumericSensor(const pldm_tid_t tid, const bool sensorDisabled,
                  std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr,
                  std::string& sensorName, std::string& associationPath);

    NumericSensor(const pldm_tid_t tid, const bool sensorDisabled,
                  std::shared_ptr<pldm_compact_numeric_sensor_pdr> pdr,
                  std::string& sensorName, std::string& associationPath);

    ~NumericSensor(){};

    /** @brief ConversionFormula is used to convert raw value to the unit
     * specified in PDR
     *
     *  @param[in] value - raw value
     *  @return double - converted value
     */
    double conversionFormula(double value);

    /** @brief UnitModifier is used to apply the unit modifier specified in PDR
     *
     *  @param[in] value - raw value
     *  @return double - converted value
     */
    double unitModifier(double value);

    /** @brief Terminus ID which the sensor belongs to */
    pldm_tid_t tid;

    /** @brief Sensor ID */
    uint16_t sensorId;

    /** @brief  The time stamp since last getSensorReading command in usec */
    uint64_t timeStamp;

    /** @brief  The time of sensor update interval in usec */
    uint64_t updateTime;

    /** @brief  sensorName */
    std::string sensorName;

    /** @brief  sensorNameSpace */
    std::string sensorNameSpace;

    /** @brief indicate if sensor is polled in priority */
    bool isPriority;

  private:
    std::unique_ptr<ValueIntf> valueIntf = nullptr;
    std::unique_ptr<ThresholdWarningIntf> thresholdWarningIntf = nullptr;
    std::unique_ptr<ThresholdCriticalIntf> thresholdCriticalIntf = nullptr;
    std::unique_ptr<AvailabilityIntf> availabilityIntf = nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;

    /** @brief Amount of hysteresis associated with the sensor thresholds */
    double hysteresis;

    /** @brief The resolution of sensor in Units */
    double resolution;

    /** @brief A constant value that is added in as part of conversion process
     * of converting a raw sensor reading to Units */
    double offset;

    /** @brief A power-of-10 multiplier for baseUnit */
    int8_t baseUnitModifier;
};
} // namespace platform_mc
} // namespace pldm
