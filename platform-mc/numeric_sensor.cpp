#include "numeric_sensor.hpp"

#include "common/utils.hpp"
#include "requester/handler.hpp"

#include <libpldm/platform.h>

#include <phosphor-logging/commit.hpp>
#include <sdbusplus/asio/property.hpp>
#include <xyz/openbmc_project/Logging/Entry/client.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/event.hpp>

#include <limits>
#include <regex>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

// This allows code to cleanly iterate through all supported
// threshold levels and directions.
static const std::array<pldm::utils::Level, 3> allThresholdLevels = {
    pldm::utils::Level::WARNING, pldm::utils::Level::CRITICAL,
    pldm::utils::Level::HARDSHUTDOWN};
static const std::array<pldm::utils::Direction, 2> allThresholdDirections = {
    pldm::utils::Direction::HIGH, pldm::utils::Direction::LOW};

inline bool NumericSensor::createInventoryPath(
    const std::string& associationPath, const std::string& sensorName,
    const uint16_t entityType, const uint16_t entityInstanceNum,
    const uint16_t containerId)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string invPath = associationPath + "/" + sensorName;
    try
    {
        entityIntf = std::make_unique<EntityIntf>(bus, invPath.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Entity interface for compact numeric sensor {PATH} error - {ERROR}",
            "PATH", invPath, "ERROR", e);
        return false;
    }
    entityIntf->entityType(entityType);
    entityIntf->entityInstanceNumber(entityInstanceNum);
    entityIntf->containerID(containerId);

    return true;
}

inline double getSensorDataValue(uint8_t sensor_data_size,
                                 union_sensor_data_size& value)
{
    double ret = std::numeric_limits<double>::quiet_NaN();
    switch (sensor_data_size)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
            ret = value.value_u8;
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            ret = value.value_s8;
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
            ret = value.value_u16;
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            ret = value.value_s16;
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
            ret = value.value_u32;
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            ret = value.value_s32;
            break;
    }
    return ret;
}

inline double getRangeFieldValue(uint8_t range_field_format,
                                 union_range_field_format& value)
{
    double ret = std::numeric_limits<double>::quiet_NaN();
    switch (range_field_format)
    {
        case PLDM_RANGE_FIELD_FORMAT_UINT8:
            ret = value.value_u8;
            break;
        case PLDM_RANGE_FIELD_FORMAT_SINT8:
            ret = value.value_s8;
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT16:
            ret = value.value_u16;
            break;
        case PLDM_RANGE_FIELD_FORMAT_SINT16:
            ret = value.value_s16;
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT32:
            ret = value.value_u32;
            break;
        case PLDM_RANGE_FIELD_FORMAT_SINT32:
            ret = value.value_s32;
            break;
        case PLDM_RANGE_FIELD_FORMAT_REAL32:
            ret = value.value_f32;
            break;
    }
    return ret;
}

void NumericSensor::setSensorUnit(uint8_t baseUnit)
{
    sensorUnit = SensorUnit::DegreesC;
    useMetricInterface = false;
    switch (baseUnit)
    {
        case PLDM_SENSOR_UNIT_DEGRESS_C:
            sensorNameSpace = "/xyz/openbmc_project/sensors/temperature/";
            sensorUnit = SensorUnit::DegreesC;
            break;
        case PLDM_SENSOR_UNIT_VOLTS:
            sensorNameSpace = "/xyz/openbmc_project/sensors/voltage/";
            sensorUnit = SensorUnit::Volts;
            break;
        case PLDM_SENSOR_UNIT_AMPS:
            sensorNameSpace = "/xyz/openbmc_project/sensors/current/";
            sensorUnit = SensorUnit::Amperes;
            break;
        case PLDM_SENSOR_UNIT_RPM:
            sensorNameSpace = "/xyz/openbmc_project/sensors/fan_pwm/";
            sensorUnit = SensorUnit::RPMS;
            break;
        case PLDM_SENSOR_UNIT_WATTS:
            sensorNameSpace = "/xyz/openbmc_project/sensors/power/";
            sensorUnit = SensorUnit::Watts;
            break;
        case PLDM_SENSOR_UNIT_JOULES:
            sensorNameSpace = "/xyz/openbmc_project/sensors/energy/";
            sensorUnit = SensorUnit::Joules;
            break;
        case PLDM_SENSOR_UNIT_PERCENTAGE:
            sensorNameSpace = "/xyz/openbmc_project/sensors/utilization/";
            sensorUnit = SensorUnit::Percent;
            break;
        case PLDM_SENSOR_UNIT_HERTZ:
            sensorNameSpace = "/xyz/openbmc_project/sensors/frequency/";
            sensorUnit = SensorUnit::Hertz;
            break;
        case PLDM_SENSOR_UNIT_COUNTS:
        case PLDM_SENSOR_UNIT_CORRECTED_ERRORS:
        case PLDM_SENSOR_UNIT_UNCORRECTABLE_ERRORS:
            sensorNameSpace = "/xyz/openbmc_project/metric/count/";
            useMetricInterface = true;
            break;
        case PLDM_SENSOR_UNIT_OEMUNIT:
            sensorNameSpace = "/xyz/openbmc_project/metric/oem/";
            useMetricInterface = true;
            break;
        default:
            lg2::error("Sensor {NAME} has Invalid baseUnit {UNIT}.", "NAME",
                       sensorName, "UNIT", baseUnit);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
            break;
    }
}

NumericSensor::NumericSensor(
    const pldm_tid_t tid, const bool sensorDisabled,
    std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr, std::string& sensorName,
    std::string& associationPath) : tid(tid), sensorName(sensorName)
{
    if (!pdr)
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    sensorId = pdr->sensor_id;
    std::string path;
    MetricUnit metricUnit = MetricUnit::Count;
    setSensorUnit(pdr->base_unit);

    path = sensorNameSpace + sensorName;
    try
    {
        std::string tmp{};
        std::string interface = SENSOR_VALUE_INTF;
        if (useMetricInterface)
        {
            interface = METRIC_VALUE_INTF;
        }
        tmp = pldm::utils::DBusHandler().getService(path.c_str(),
                                                    interface.c_str());

        if (!tmp.empty())
        {
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                TooManyResources();
        }
    }
    catch (const std::exception&)
    {
        /* The sensor object path is not created */
    }

    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        associationDefinitionsIntf =
            std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create association interface for numeric sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    associationDefinitionsIntf->associations(
        {{"chassis", "all_sensors", associationPath}});

    double maxValue =
        getSensorDataValue(pdr->sensor_data_size, pdr->max_readable);
    double minValue =
        getSensorDataValue(pdr->sensor_data_size, pdr->min_readable);
    hysteresis = getSensorDataValue(pdr->sensor_data_size, pdr->hysteresis);

    bool hasCriticalThresholds = false;
    bool hasWarningThresholds = false;
    bool hasFatalThresholds = false;
    double criticalHigh = std::numeric_limits<double>::quiet_NaN();
    double criticalLow = std::numeric_limits<double>::quiet_NaN();
    double warningHigh = std::numeric_limits<double>::quiet_NaN();
    double warningLow = std::numeric_limits<double>::quiet_NaN();
    double fatalHigh = std::numeric_limits<double>::quiet_NaN();
    double fatalLow = std::numeric_limits<double>::quiet_NaN();

    if (pdr->supported_thresholds.bits.bit0)
    {
        hasWarningThresholds = true;
        warningHigh =
            getRangeFieldValue(pdr->range_field_format, pdr->warning_high);
    }

    if (pdr->supported_thresholds.bits.bit3)
    {
        hasWarningThresholds = true;
        warningLow =
            getRangeFieldValue(pdr->range_field_format, pdr->warning_low);
    }

    if (pdr->supported_thresholds.bits.bit1)
    {
        hasCriticalThresholds = true;
        criticalHigh =
            getRangeFieldValue(pdr->range_field_format, pdr->critical_high);
    }

    if (pdr->supported_thresholds.bits.bit4)
    {
        hasCriticalThresholds = true;
        criticalLow =
            getRangeFieldValue(pdr->range_field_format, pdr->critical_low);
    }
    if (pdr->supported_thresholds.bits.bit2)
    {
        hasFatalThresholds = true;
        fatalHigh =
            getRangeFieldValue(pdr->range_field_format, pdr->fatal_high);
    }
    if (pdr->supported_thresholds.bits.bit5)
    {
        hasFatalThresholds = true;
        fatalLow = getRangeFieldValue(pdr->range_field_format, pdr->fatal_low);
    }

    resolution = pdr->resolution;
    offset = pdr->offset;
    baseUnitModifier = pdr->unit_modifier;
    timeStamp = 0;

    /**
     * DEFAULT_SENSOR_UPDATER_INTERVAL is in milliseconds
     * updateTime is in microseconds
     */
    updateTime = static_cast<uint64_t>(DEFAULT_SENSOR_UPDATER_INTERVAL * 1000);
    if (std::isfinite(pdr->update_interval))
    {
        updateTime = pdr->update_interval * 1000000;
    }

    if (!useMetricInterface)
    {
        try
        {
            valueIntf = std::make_unique<ValueIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create Value interface for numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        valueIntf->maxValue(unitModifier(conversionFormula(maxValue)));
        valueIntf->minValue(unitModifier(conversionFormula(minValue)));
        valueIntf->unit(sensorUnit);
    }
    else
    {
        try
        {
            metricIntf = std::make_unique<MetricIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create Metric interface for numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        metricIntf->maxValue(unitModifier(conversionFormula(maxValue)));
        metricIntf->minValue(unitModifier(conversionFormula(minValue)));
        metricIntf->unit(metricUnit);
    }

    hysteresis = unitModifier(conversionFormula(hysteresis));

    if (!createInventoryPath(associationPath, sensorName, pdr->entity_type,
                             pdr->entity_instance_num, pdr->container_id))
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    try
    {
        availabilityIntf =
            std::make_unique<AvailabilityIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Availability interface for numeric sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    availabilityIntf->available(true);

    try
    {
        operationalStatusIntf =
            std::make_unique<OperationalStatusIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Operation status interface for numeric sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    operationalStatusIntf->functional(!sensorDisabled);

    if (hasWarningThresholds && !useMetricInterface)
    {
        try
        {
            thresholdWarningIntf =
                std::make_unique<ThresholdWarningIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create Threshold warning interface for numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        thresholdWarningIntf->warningHigh(unitModifier(warningHigh));
        thresholdWarningIntf->warningLow(unitModifier(warningLow));
    }

    if (hasCriticalThresholds && !useMetricInterface)
    {
        try
        {
            thresholdCriticalIntf =
                std::make_unique<ThresholdCriticalIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create Threshold critical interface for numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        thresholdCriticalIntf->criticalHigh(unitModifier(criticalHigh));
        thresholdCriticalIntf->criticalLow(unitModifier(criticalLow));
    }

    if (hasFatalThresholds && !useMetricInterface)
    {
        try
        {
            thresholdHardShutdownIntf =
                std::make_unique<ThresholdHardShutdownIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create HardShutdown threshold interface for numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        thresholdHardShutdownIntf->hardShutdownHigh(unitModifier(fatalHigh));
        thresholdHardShutdownIntf->hardShutdownLow(unitModifier(fatalLow));
    }
}

NumericSensor::NumericSensor(
    const pldm_tid_t tid, const bool sensorDisabled,
    std::shared_ptr<pldm_compact_numeric_sensor_pdr> pdr,
    std::string& sensorName, std::string& associationPath) :
    tid(tid), sensorName(sensorName)
{
    if (!pdr)
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    sensorId = pdr->sensor_id;
    std::string path;
    MetricUnit metricUnit = MetricUnit::Count;
    setSensorUnit(pdr->base_unit);

    path = sensorNameSpace + sensorName;
    try
    {
        std::string tmp{};
        std::string interface = SENSOR_VALUE_INTF;
        if (useMetricInterface)
        {
            interface = METRIC_VALUE_INTF;
        }
        tmp = pldm::utils::DBusHandler().getService(path.c_str(),
                                                    interface.c_str());

        if (!tmp.empty())
        {
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                TooManyResources();
        }
    }
    catch (const std::exception&)
    {
        /* The sensor object path is not created */
    }

    auto& bus = pldm::utils::DBusHandler::getBus();
    try
    {
        associationDefinitionsIntf =
            std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Association interface for compact numeric sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    associationDefinitionsIntf->associations(
        {{"chassis", "all_sensors", associationPath.c_str()}});

    double maxValue = std::numeric_limits<double>::quiet_NaN();
    double minValue = std::numeric_limits<double>::quiet_NaN();
    bool hasWarningThresholds = false;
    bool hasCriticalThresholds = false;
    bool hasFatalThresholds = false;
    double criticalHigh = std::numeric_limits<double>::quiet_NaN();
    double criticalLow = std::numeric_limits<double>::quiet_NaN();
    double warningHigh = std::numeric_limits<double>::quiet_NaN();
    double warningLow = std::numeric_limits<double>::quiet_NaN();
    double fatalHigh = std::numeric_limits<double>::quiet_NaN();
    double fatalLow = std::numeric_limits<double>::quiet_NaN();

    if (pdr->range_field_support.bits.bit0)
    {
        hasWarningThresholds = true;
        warningHigh = pdr->warning_high;
    }
    if (pdr->range_field_support.bits.bit1)
    {
        hasWarningThresholds = true;
        warningLow = pdr->warning_low;
    }

    if (pdr->range_field_support.bits.bit2)
    {
        hasCriticalThresholds = true;
        criticalHigh = pdr->critical_high;
    }

    if (pdr->range_field_support.bits.bit3)
    {
        hasCriticalThresholds = true;
        criticalLow = pdr->critical_low;
    }
    if (pdr->range_field_support.bits.bit4)
    {
        hasFatalThresholds = true;
        fatalHigh = pdr->fatal_high;
    }
    if (pdr->range_field_support.bits.bit5)
    {
        hasFatalThresholds = true;
        fatalLow = pdr->fatal_low;
    }

    resolution = std::numeric_limits<double>::quiet_NaN();
    offset = std::numeric_limits<double>::quiet_NaN();
    baseUnitModifier = pdr->unit_modifier;
    timeStamp = 0;
    hysteresis = 0;

    /**
     * DEFAULT_SENSOR_UPDATER_INTERVAL is in milliseconds
     * updateTime is in microseconds
     */
    updateTime = static_cast<uint64_t>(DEFAULT_SENSOR_UPDATER_INTERVAL * 1000);

    if (!useMetricInterface)
    {
        try
        {
            valueIntf = std::make_unique<ValueIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create Value interface for compact numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        valueIntf->maxValue(unitModifier(conversionFormula(maxValue)));
        valueIntf->minValue(unitModifier(conversionFormula(minValue)));
        valueIntf->unit(sensorUnit);
    }
    else
    {
        try
        {
            metricIntf = std::make_unique<MetricIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create Metric interface for compact numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        metricIntf->maxValue(unitModifier(conversionFormula(maxValue)));
        metricIntf->minValue(unitModifier(conversionFormula(minValue)));
        metricIntf->unit(metricUnit);
    }

    hysteresis = unitModifier(conversionFormula(hysteresis));

    if (!createInventoryPath(associationPath, sensorName, pdr->entity_type,
                             pdr->entity_instance, pdr->container_id))
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    try
    {
        availabilityIntf =
            std::make_unique<AvailabilityIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Availability interface for compact numeric sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    availabilityIntf->available(true);

    try
    {
        operationalStatusIntf =
            std::make_unique<OperationalStatusIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Operational status interface for compact numeric sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    operationalStatusIntf->functional(!sensorDisabled);

    if (hasWarningThresholds && !useMetricInterface)
    {
        try
        {
            thresholdWarningIntf =
                std::make_unique<ThresholdWarningIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create Warning threshold interface for compact numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        thresholdWarningIntf->warningHigh(unitModifier(warningHigh));
        thresholdWarningIntf->warningLow(unitModifier(warningLow));
    }

    if (hasCriticalThresholds && !useMetricInterface)
    {
        try
        {
            thresholdCriticalIntf =
                std::make_unique<ThresholdCriticalIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create Critical threshold interface for compact numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        thresholdCriticalIntf->criticalHigh(unitModifier(criticalHigh));
        thresholdCriticalIntf->criticalLow(unitModifier(criticalLow));
    }

    if (hasFatalThresholds && !useMetricInterface)
    {
        try
        {
            thresholdHardShutdownIntf =
                std::make_unique<ThresholdHardShutdownIntf>(bus, path.c_str());
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create HardShutdown threshold interface for numeric sensor {PATH} error - {ERROR}",
                "PATH", path, "ERROR", e);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
        }
        thresholdHardShutdownIntf->hardShutdownHigh(unitModifier(fatalHigh));
        thresholdHardShutdownIntf->hardShutdownLow(unitModifier(fatalLow));
    }
}

double NumericSensor::conversionFormula(double value)
{
    double convertedValue = value;
    if (std::isfinite(resolution))
    {
        convertedValue *= resolution;
    }
    if (std::isfinite(offset))
    {
        convertedValue += offset;
    }
    return convertedValue;
}

double NumericSensor::unitModifier(double value)
{
    if (!std::isfinite(value))
    {
        return value;
    }
    return value * std::pow(10, baseUnitModifier);
}

void NumericSensor::updateReading(bool available, bool functional, double value)
{
    if (!availabilityIntf || !operationalStatusIntf ||
        (!useMetricInterface && !valueIntf) ||
        (useMetricInterface && !metricIntf))
    {
        lg2::error(
            "Failed to update sensor {NAME} D-Bus interface don't exist.",
            "NAME", sensorName);
        return;
    }
    availabilityIntf->available(available);
    operationalStatusIntf->functional(functional);
    double curValue = 0;
    if (!useMetricInterface)
    {
        curValue = valueIntf->value();
    }
    else
    {
        curValue = metricIntf->value();
    }

    double newValue = std::numeric_limits<double>::quiet_NaN();
    if (functional && available)
    {
        newValue = unitModifier(conversionFormula(value));
        if (std::isfinite(newValue) || std::isfinite(curValue))
        {
            if (!useMetricInterface)
            {
                valueIntf->value(newValue);
                updateThresholds();
            }
            else
            {
                metricIntf->value(newValue);
            }
        }
    }
    else
    {
        if (newValue != curValue &&
            (std::isfinite(newValue) || std::isfinite(curValue)))
        {
            if (!useMetricInterface)
            {
                valueIntf->value(std::numeric_limits<double>::quiet_NaN());
            }
            else
            {
                metricIntf->value(std::numeric_limits<double>::quiet_NaN());
            }
        }
    }
}

void NumericSensor::handleErrGetSensorReading()
{
    if (!operationalStatusIntf || (!useMetricInterface && !valueIntf) ||
        (useMetricInterface && !metricIntf))
    {
        lg2::error(
            "Failed to update sensor {NAME} D-Bus interfaces don't exist.",
            "NAME", sensorName);
        return;
    }
    operationalStatusIntf->functional(false);
    if (!useMetricInterface)
    {
        valueIntf->value(std::numeric_limits<double>::quiet_NaN());
    }
    else
    {
        metricIntf->value(std::numeric_limits<double>::quiet_NaN());
    }
}

bool NumericSensor::checkThreshold(bool alarm, bool direction, double value,
                                   double threshold, double hyst)
{
    if (direction)
    {
        if (value >= threshold)
        {
            return true;
        }
        if (value < (threshold - hyst))
        {
            return false;
        }
    }
    else
    {
        if (value <= threshold)
        {
            return true;
        }
        if (value > (threshold + hyst))
        {
            return false;
        }
    }
    return alarm;
}

bool NumericSensor::hasThresholdAlarm()
{
    bool alarm = false;
    for (auto level : allThresholdLevels)
    {
        for (auto direction : allThresholdDirections)
        {
            alarm |= getThresholdAlarm(level, direction);
        }
    }
    return alarm;
}

void NumericSensor::setWarningThresholdAlarm(pldm::utils::Direction direction,
                                             double value, bool newAlarm)
{
    if (direction == pldm::utils::Direction::HIGH)
    {
        thresholdWarningIntf->warningAlarmHigh(newAlarm);
        if (newAlarm)
        {
            thresholdWarningIntf->warningHighAlarmAsserted(value);
        }
        else
        {
            thresholdWarningIntf->warningHighAlarmDeasserted(value);
        }
    }
    else
    {
        thresholdWarningIntf->warningAlarmLow(newAlarm);
        if (newAlarm)
        {
            thresholdWarningIntf->warningLowAlarmAsserted(value);
        }
        else
        {
            thresholdWarningIntf->warningLowAlarmDeasserted(value);
        }
    }
}

void NumericSensor::setCriticalThresholdAlarm(pldm::utils::Direction direction,
                                              double value, bool newAlarm)
{
    if (direction == pldm::utils::Direction::HIGH)
    {
        thresholdCriticalIntf->criticalAlarmHigh(newAlarm);
        if (newAlarm)
        {
            thresholdCriticalIntf->criticalHighAlarmAsserted(value);
        }
        else
        {
            thresholdCriticalIntf->criticalHighAlarmDeasserted(value);
        }
    }
    else
    {
        thresholdCriticalIntf->criticalAlarmLow(newAlarm);
        if (newAlarm)
        {
            thresholdCriticalIntf->criticalLowAlarmAsserted(value);
        }
        else
        {
            thresholdCriticalIntf->criticalLowAlarmDeasserted(value);
        }
    }
}

void NumericSensor::setHardShutdownThresholdAlarm(
    pldm::utils::Direction direction, double value, bool newAlarm)
{
    if (direction == pldm::utils::Direction::HIGH)
    {
        thresholdHardShutdownIntf->hardShutdownAlarmHigh(newAlarm);
        if (newAlarm)
        {
            thresholdHardShutdownIntf->hardShutdownHighAlarmAsserted(value);
        }
        else
        {
            thresholdHardShutdownIntf->hardShutdownHighAlarmDeasserted(value);
        }
    }
    else
    {
        thresholdHardShutdownIntf->hardShutdownAlarmLow(newAlarm);
        if (newAlarm)
        {
            thresholdHardShutdownIntf->hardShutdownLowAlarmAsserted(value);
        }
        else
        {
            thresholdHardShutdownIntf->hardShutdownLowAlarmDeasserted(value);
        }
    }
}

int NumericSensor::setThresholdAlarm(pldm::utils::Level level,
                                     pldm::utils::Direction direction,
                                     double value, bool newAlarm)
{
    if (!isThresholdValid(level, direction))
    {
        lg2::error(
            "Error:Trigger sensor warning event for non warning threshold sensors {NAME}",
            "NAME", sensorName);
        return PLDM_ERROR;
    }
    auto alarm = getThresholdAlarm(level, direction);
    if (alarm == newAlarm)
    {
        return PLDM_SUCCESS;
    }
    switch (level)
    {
        case pldm::utils::Level::WARNING:
            setWarningThresholdAlarm(direction, value, newAlarm);
            break;
        case pldm::utils::Level::CRITICAL:
            setCriticalThresholdAlarm(direction, value, newAlarm);
            break;
        case pldm::utils::Level::HARDSHUTDOWN:
            setHardShutdownThresholdAlarm(direction, value, newAlarm);
            break;
        default:
            return PLDM_ERROR;
    }
    if (newAlarm)
    {
        createThresholdLog(level, direction, value);
    }
    else
    {
        auto& log = assertedLog[{level, direction}];
        if (log.has_value())
        {
            clearThresholdLog(log);
        }
        // If all alarms have cleared. Log normal range.
        if (!hasThresholdAlarm())
        {
            createNormalRangeLog(value);
        }
    }
    return PLDM_SUCCESS;
}

void NumericSensor::clearThresholdLog(
    std::optional<sdbusplus::message::object_path>& log)
{
    if (!log)
    {
        return;
    }
    try
    {
        /* empty log entries are returned by commit() if the
          requested log is being filtered out */
        if (!log->str.empty())
        {
            lg2::resolve(*log);
        }
    }
    catch (std::exception& ec)
    {
        lg2::error("Error trying to resolve: {LOG} : {ERROR}", "LOG", log->str,
                   "ERROR", ec);
    }
    log.reset();
}

/** @brief helper function template to create a threshold log
 *
 * @tparam[in] errorObj - The error object of the log we want to create.
 * @param[in] sensorObjPath - The object path of the sensor.
 * @param[in] value - The current value of the sensor.
 * @param[in] sensorUnit - The units of the sensor.
 * @param[in] threshold - The threshold value.
 *
 * @return optional object holding the object path of the created
 * log entry. If the log entry is being filtered, we would return
 * a optional holding an empty string in the object path. This ensures
 * we follow our state machine properly even if the log is being filtered.
 */
template <typename errorObj>
auto logThresholdHelper(const std::string& sensorObjPath, double value,
                        SensorUnit sensorUnit, double threshold)
    -> std::optional<sdbusplus::message::object_path>
{
    return lg2::commit(
        errorObj("SENSOR_NAME", sensorObjPath, "READING_VALUE", value, "UNITS",
                 sensorUnit, "THRESHOLD_VALUE", threshold));
}

void NumericSensor::createThresholdLog(
    pldm::utils::Level level, pldm::utils::Direction direction, double value)
{
    namespace Errors =
        sdbusplus::error::xyz::openbmc_project::sensor::Threshold;
    /* Map from threshold level+direction to a an instantiation of
     * logThresholdHelper with the required error object class */
    static const std::map<
        std::tuple<pldm::utils::Level, pldm::utils::Direction>,
        std::function<std::optional<sdbusplus::message::object_path>(
            const std::string&, double, SensorUnit, double)>>
        thresholdEventMap = {
            {{pldm::utils::Level::WARNING, pldm::utils::Direction::HIGH},
             &logThresholdHelper<Errors::ReadingAboveUpperWarningThreshold>},
            {{pldm::utils::Level::WARNING, pldm::utils::Direction::LOW},
             &logThresholdHelper<Errors::ReadingBelowLowerWarningThreshold>},
            {{pldm::utils::Level::CRITICAL, pldm::utils::Direction::HIGH},
             &logThresholdHelper<Errors::ReadingAboveUpperCriticalThreshold>},
            {{pldm::utils::Level::CRITICAL, pldm::utils::Direction::LOW},
             &logThresholdHelper<Errors::ReadingBelowLowerCriticalThreshold>},
            {{pldm::utils::Level::HARDSHUTDOWN, pldm::utils::Direction::HIGH},
             &logThresholdHelper<
                 Errors::ReadingAboveUpperHardShutdownThreshold>},
            {{pldm::utils::Level::HARDSHUTDOWN, pldm::utils::Direction::LOW},
             &logThresholdHelper<
                 Errors::ReadingBelowLowerHardShutdownThreshold>},
        };

    std::string sensorObjPath = sensorNameSpace + sensorName;
    double threshold = getThreshold(level, direction);
    try
    {
        auto helper = thresholdEventMap.at({level, direction});
        assertedLog[{level, direction}] =
            helper(sensorObjPath, value, sensorUnit, threshold);
    }
    catch (std::exception& ec)
    {
        lg2::error(
            "Unable to create threshold log entry for {OBJPATH}: {ERROR}",
            "OBJPATH", sensorObjPath, "ERROR", ec);
    }
}

void NumericSensor::createNormalRangeLog(double value)
{
    namespace Events =
        sdbusplus::event::xyz::openbmc_project::sensor::Threshold;
    std::string objPath = sensorNameSpace + sensorName;
    try
    {
        lg2::commit(Events::SensorReadingNormalRange(
            "SENSOR_NAME", objPath, "READING_VALUE", value, "UNITS",
            sensorUnit));
    }
    catch (std::exception& ec)
    {
        lg2::error(
            "Unable to create SensorReadingNormalRange log entry for {OBJPATH}: {ERROR}",
            "OBJPATH", objPath, "ERROR", ec);
    }
}

void NumericSensor::updateThresholds()
{
    double value = std::numeric_limits<double>::quiet_NaN();

    if ((!useMetricInterface && !valueIntf) ||
        (useMetricInterface && !metricIntf))
    {
        lg2::error(
            "Failed to update thresholds sensor {NAME} D-Bus interfaces don't exist.",
            "NAME", sensorName);
        return;
    }
    if (!useMetricInterface)
    {
        value = valueIntf->value();
    }
    else
    {
        value = metricIntf->value();
    }

    for (auto level : allThresholdLevels)
    {
        for (auto direction : allThresholdDirections)
        {
            auto threshold = getThreshold(level, direction);
            if (!std::isfinite(threshold))
            {
                continue;
            }
            auto alarm = getThresholdAlarm(level, direction);
            auto newAlarm =
                checkThreshold(alarm, direction == pldm::utils::Direction::HIGH,
                               value, threshold, hysteresis);
            setThresholdAlarm(level, direction, value, newAlarm);
        }
    }
}

int NumericSensor::triggerThresholdEvent(
    pldm::utils::Level eventType, pldm::utils::Direction direction,
    double rawValue, bool newAlarm, bool assert)
{
    if (!valueIntf)
    {
        lg2::error(
            "Failed to update thresholds sensor {NAME} D-Bus interfaces don't exist.",
            "NAME", sensorName);
        return PLDM_ERROR;
    }

    auto value = unitModifier(conversionFormula(rawValue));
    lg2::error(
        "triggerThresholdEvent eventType {TID}, direction {SID} value {VAL} newAlarm {PSTATE} assert {ESTATE}",
        "TID", eventType, "SID", direction, "VAL", value, "PSTATE", newAlarm,
        "ESTATE", assert);

    return setThresholdAlarm(eventType, direction, value, newAlarm);
}
} // namespace platform_mc
} // namespace pldm
