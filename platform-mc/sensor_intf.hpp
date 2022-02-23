#pragma once

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "common/types.hpp"
#include "requester/handler.hpp"

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <iostream>

namespace pldm
{
namespace platform_mc
{

using namespace std::chrono;
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

/** @class SensorIntf
 *
 * This abstract class defines the APIs for sensor class has common
 * interface to execute function from different Manager Classes
 */
class SensorIntf
{
  public:
    SensorIntf() = delete;
    SensorIntf(const SensorIntf&) = delete;
    SensorIntf(SensorIntf&&) = delete;
    SensorIntf& operator=(const SensorIntf&) = delete;
    SensorIntf& operator=(SensorIntf&&) = delete;
    virtual ~SensorIntf() = default;

    explicit SensorIntf(const tid_t tid, const bool,
                        const std::vector<uint8_t>& pdrData, uint16_t sensorId,
                        std::string& sensorName, std::string& associationPath) :
        tid(tid),
        sensorPdr(pdrData), sensorId(sensorId), sensorName(sensorName),
        associationPath(associationPath)
    {}

    /** @brief ConversionFormula is used to convert raw value to the units
     * specified in the PDR
     *
     *
     *  @param[in] value - raw value
     *  @return double - converted value
     */
    virtual double conversionFormula(double value);

    /** @brief UnitModifier is used to apply the unit modifier specified in PDR
     *
     *  @param[in] value - raw value
     *  @return double - converted value
     */
    virtual double unitModifier(double value);

    /** @brief The function called by Sensor Manager to set sensor to
     * error status.
     */
    virtual void handleErrGetSensorReading();

    /** @brief Updating the sensor status to D-Bus interface
     */
    virtual void updateReading(bool available, bool functional, double value);

    /** @brief Check if value is over threshold.
     *
     *  @param[in] alarm - previous alarm state
     *  @param[in] direction - upper or lower threshold checking
     *  @param[in] value - raw value
     *  @param[in] threshold - threshold value
     *  @param[in] hyst - hysteresis value
     *  @return bool - new alarm state
     */
    virtual bool checkThreshold(bool alarm, bool direction, double value,
                                double threshold, double hyst);

    virtual void updateThresholds();

    virtual std::vector<uint8_t> getReadingRequest();

    virtual int
        handleReadingResponse([[maybe_unused]] const pldm_msg* responseMsg,
                              [[maybe_unused]] size_t responseLen);

    /** @brief Terminus ID which the sensor belongs to */
    tid_t tid;

    /** @brief PLDM PDR Data */
    std::vector<uint8_t> sensorPdr;

    /** @brief Sensor ID */
    uint16_t sensorId;

    /** @brief Sensor Name */
    std::string sensorName;

    /** @brief association Path */
    std::string associationPath;

    /** @brief  The time since last getSensorReading command in usec */
    uint64_t elapsedTime;

    /** @brief  The time of sensor update interval in usec */
    uint64_t updateTime = 1000000;

  protected:
    std::unique_ptr<ValueIntf> valueIntf = nullptr;
    std::unique_ptr<ThresholdWarningIntf> thresholdWarningIntf = nullptr;
    std::unique_ptr<ThresholdCriticalIntf> thresholdCriticalIntf = nullptr;
    std::unique_ptr<AvailabilityIntf> availabilityIntf = nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;

    /** @brief Amount of hysteresis associated with the sensor thresholds */
    double hysteresis = 0;

    /** @brief The resolution of sensor in Units */
    double resolution = 1;

    /** @brief A constant value that is added in as part of conversion process
     * of converting a raw sensor reading to Units */
    double offset = 0;

    /** @brief A power-of-10 multiplier for baseUnit */
    int8_t baseUnitModifier = 0;
};

} // namespace platform_mc
} // namespace pldm
