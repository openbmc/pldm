#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/platform.h>
#include <libpldm/pldm.h>

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Source/PLDM/Entity/server.hpp>
#include <xyz/openbmc_project/Metric/Value/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/HardShutdown/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <string>

namespace pldm
{
namespace platform_mc
{

constexpr const char* SENSOR_VALUE_INTF = "xyz.openbmc_project.Sensor.Value";
constexpr const char* METRIC_VALUE_INTF = "xyz.openbmc_project.Metric.Value";

using SensorUnit = sdbusplus::xyz::openbmc_project::Sensor::server::Value::Unit;
using ValueIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::server::Value>;
using MetricUnit = sdbusplus::xyz::openbmc_project::Metric::server::Value::Unit;
using MetricIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Metric::server::Value>;
using ThresholdWarningIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Warning>;
using ThresholdCriticalIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Critical>;
using ThresholdHardShutdownIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::HardShutdown>;
using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;
using AvailabilityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Availability>;
using AssociationDefinitionsInft = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
using EntityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Source::PLDM::server::Entity>;

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

    ~NumericSensor() {};

    /** @brief The function called by Sensor Manager to set sensor to
     * error status.
     */
    void handleErrGetSensorReading();

    /** @brief Updating the sensor status to D-Bus interface
     */
    void updateReading(bool available, bool functional, double value = 0);

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

    /** @brief Check if value is over threshold.
     *
     *  @param[in] alarm - previous alarm state
     *  @param[in] direction - upper or lower threshold checking
     *  @param[in] value - raw value
     *  @param[in] threshold - threshold value
     *  @param[in] hyst - hysteresis value
     *  @return bool - new alarm state
     */
    bool checkThreshold(bool alarm, bool direction, double value,
                        double threshold, double hyst);

    /** @brief Updating the association to D-Bus interface
     *  @param[in] inventoryPath - inventory path of the entity
     */
    inline void setInventoryPath(const std::string& inventoryPath)
    {
        if (associationDefinitionsIntf)
        {
            associationDefinitionsIntf->associations(
                {{"chassis", "all_sensors", inventoryPath}});
        }
    }

    /** @brief Get Upper Critical threshold
     *
     *  @return double - Upper Critical threshold
     */
    double getThresholdUpperCritical()
    {
        if (thresholdCriticalIntf)
        {
            return thresholdCriticalIntf->criticalHigh();
        }
        else
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
    };

    /** @brief Get Lower Critical threshold
     *
     *  @return double - Lower Critical threshold
     */
    double getThresholdLowerCritical()
    {
        if (thresholdCriticalIntf)
        {
            return thresholdCriticalIntf->criticalLow();
        }
        else
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
    };

    /** @brief Get Upper Warning threshold
     *
     *  @return double - Upper Warning threshold
     */
    double getThresholdUpperWarning()
    {
        if (thresholdWarningIntf)
        {
            return thresholdWarningIntf->warningHigh();
        }
        else
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
    };

    /** @brief Get Lower Warning threshold
     *
     *  @return double - Lower Warning threshold
     */
    double getThresholdLowerWarning()
    {
        if (thresholdWarningIntf)
        {
            return thresholdWarningIntf->warningLow();
        }
        else
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
    };

    /** @brief Get Upper HardShutdown threshold
     *
     *  @return double - Upper HardShutdown threshold
     */
    double getThresholdUpperHardShutdown()
    {
        if (thresholdHardShutdownIntf)
        {
            return thresholdHardShutdownIntf->hardShutdownHigh();
        }
        else
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
    };

    /** @brief Get Lower HardShutdown threshold
     *
     *  @return double - Lower HardShutdown threshold
     */
    double getThresholdLowerHardShutdown()
    {
        if (thresholdHardShutdownIntf)
        {
            return thresholdHardShutdownIntf->hardShutdownLow();
        }
        else
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
    };

    /** @brief Get threshold given level and direction
     *
     * @param[in] level - The threshold level (WARNING/CRITICAL/etc)
     * @param[in] direction - The threshold direction (HIGH/LOW)
     *
     * @return double - The requested threshold.
     */
    double getThreshold(pldm::utils::Level level,
                        pldm::utils::Direction direction)
    {
        if (direction != pldm::utils::Direction::HIGH &&
            direction != pldm::utils::Direction::LOW)
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
        switch (level)
        {
            case pldm::utils::Level::WARNING:
                return direction == pldm::utils::Direction::HIGH
                           ? getThresholdUpperWarning()
                           : getThresholdLowerWarning();
            case pldm::utils::Level::CRITICAL:
                return direction == pldm::utils::Direction::HIGH
                           ? getThresholdUpperCritical()
                           : getThresholdLowerCritical();
            case pldm::utils::Level::HARDSHUTDOWN:
                return direction == pldm::utils::Direction::HIGH
                           ? getThresholdUpperHardShutdown()
                           : getThresholdLowerHardShutdown();
            default:
                break;
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    /* @brief returns true if the given threshold at level/direction is defined.
     *
     * @param[in] level - The threshold level (WARNING/CRITICAL/etc)
     * @param[in] direction - The threshold direction (HIGH/LOW)
     *
     * @return true if the threshold is valid
     */
    bool isThresholdValid(pldm::utils::Level level,
                          pldm::utils::Direction direction)
    {
        return std::isfinite(getThreshold(level, direction));
    }

    /* @brief Get the alarm status of the given threshold
     *
     * @param[in] level - The threshold level (WARNING/CRITICAL/etc)
     * @param[in] direction - The threshold direction (HIGH/LOW)
     *
     * @return true if the current alarm status is asserted.
     */
    bool getThresholdAlarm(pldm::utils::Level level,
                           pldm::utils::Direction direction)
    {
        if (!isThresholdValid(level, direction))
        {
            return false;
        }
        switch (level)
        {
            case pldm::utils::Level::WARNING:
                return direction == pldm::utils::Direction::HIGH
                           ? thresholdWarningIntf->warningAlarmHigh()
                           : thresholdWarningIntf->warningAlarmLow();
            case pldm::utils::Level::CRITICAL:
                return direction == pldm::utils::Direction::HIGH
                           ? thresholdCriticalIntf->criticalAlarmHigh()
                           : thresholdCriticalIntf->criticalAlarmLow();
            case pldm::utils::Level::HARDSHUTDOWN:
                return direction == pldm::utils::Direction::HIGH
                           ? thresholdHardShutdownIntf->hardShutdownAlarmHigh()
                           : thresholdHardShutdownIntf->hardShutdownAlarmLow();
            default:
                break;
        }
        return false;
    }

    /* @brief Returns true if at least one threshold alarm is set
     *
     * @return true if at least one threshold alarm is set
     */
    bool hasThresholdAlarm();

    /* @brief raises the alarm on the warning threshold
     *
     * @param[in] direction - The threshold direction (HIGH/LOW)
     * @param[in] value - The current numeric sensor reading
     * @param[in] asserted - true if we want to set the alarm, false
     *                       if we want to clear it.
     *
     * @return PLDM_SUCCESS or a valid error code.
     */
    void setWarningThresholdAlarm(pldm::utils::Direction direction,
                                  double value, bool asserted);

    /* @brief raises the alarm on the critical threshold
     *
     * @param[in] direction - The threshold direction (HIGH/LOW)
     * @param[in] value - The current numeric sensor reading
     * @param[in] asserted - true if we want to set the alarm, false
     *                       if we want to clear it.
     *
     * @return PLDM_SUCCESS or a valid error code.
     */
    void setCriticalThresholdAlarm(pldm::utils::Direction direction,
                                   double value, bool asserted);

    /* @brief raises the alarm on the hard-shutdown threshold
     *
     * @param[in] direction - The threshold direction (HIGH/LOW)
     * @param[in] value - The current numeric sensor reading
     * @param[in] asserted - true if we want to set the alarm, false
     *                       if we want to clear it.
     *
     * @return PLDM_SUCCESS or a valid error code.
     */
    void setHardShutdownThresholdAlarm(pldm::utils::Direction direction,
                                       double value, bool asserted);

    /* @brief raises the alarm on the threshold
     *
     * @param[in] level - The threshold level (WARNING/CRITICAL/etc)
     * @param[in] direction - The threshold direction (HIGH/LOW)
     * @param[in] value - The current numeric sensor reading
     * @param[in] asserted - true if we want to set the alarm, false
     *                       if we want to clear it.
     *
     * @return PLDM_SUCCESS or a valid error code.
     */
    int setThresholdAlarm(pldm::utils::Level level,
                          pldm::utils::Direction direction, double value,
                          bool asserted);

    /** @brief Check if value is over threshold.
     *
     *  @param[in] eventType - event level in pldm::utils::Level
     *  @param[in] direction - direction type in pldm::utils::Direction
     *  @param[in] rawValue - sensor raw value
     *  @param[in] newAlarm - trigger alarm true/false
     *  @param[in] assert - event type asserted/deasserted
     *
     *  @return PLDM completion code
     */
    int triggerThresholdEvent(pldm::utils::Level eventType,
                              pldm::utils::Direction direction, double rawValue,
                              bool newAlarm, bool assert);

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

    /** @brief Sensor Unit */
    SensorUnit sensorUnit;

  private:
    void clearThresholdLog(std::optional<sdbusplus::message::object_path>& log);
    void createNormalRangeLog(double value);
    void createThresholdLog(pldm::utils::Level level,
                            pldm::utils::Direction direction, double value);
    /**
     * @brief Check sensor reading if any threshold has been crossed and update
     * Threshold interfaces accordingly
     */
    void updateThresholds();

    /**
     * @brief Update the object units based on the PDR baseUnit
     */
    void setSensorUnit(uint8_t baseUnit);

    /** @brief Create the sensor inventory path.
     *
     *  @param[in] associationPath - sensor association path
     *  @param[in] sensorName - sensor name
     *  @param[in] entityType - sensor PDR entity type
     *  @param[in] entityInstanceNum - sensor PDR entity instance number
     *  @param[in] containerId - sensor PDR entity container ID
     *
     *  @return True when success otherwise return False
     */
    inline bool createInventoryPath(
        const std::string& associationPath, const std::string& sensorName,
        const uint16_t entityType, const uint16_t entityInstanceNum,
        const uint16_t containerId);

    std::unique_ptr<MetricIntf> metricIntf = nullptr;
    std::unique_ptr<ValueIntf> valueIntf = nullptr;
    std::unique_ptr<ThresholdWarningIntf> thresholdWarningIntf = nullptr;
    std::unique_ptr<ThresholdCriticalIntf> thresholdCriticalIntf = nullptr;
    std::unique_ptr<ThresholdHardShutdownIntf> thresholdHardShutdownIntf =
        nullptr;
    std::unique_ptr<AvailabilityIntf> availabilityIntf = nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;
    std::unique_ptr<EntityIntf> entityIntf = nullptr;

    /** @brief Amount of hysteresis associated with the sensor thresholds */
    double hysteresis;

    /** @brief The resolution of sensor in Units */
    double resolution;

    /** @brief A constant value that is added in as part of conversion process
     * of converting a raw sensor reading to Units */
    double offset;

    /** @brief A power-of-10 multiplier for baseUnit */
    int8_t baseUnitModifier;
    bool useMetricInterface = false;

    std::map<std::tuple<pldm::utils::Level, pldm::utils::Direction>,
             std::optional<sdbusplus::message::object_path>>
        assertedLog;
};
} // namespace platform_mc
} // namespace pldm
