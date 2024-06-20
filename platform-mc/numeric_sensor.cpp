#include "numeric_sensor.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"
#include "requester/handler.hpp"

#include <limits>
#include <regex>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

NumericSensor::NumericSensor(const pldm_tid_t tid, const bool sensorDisabled,
                             std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr,
                             std::string& sensorName,
                             std::string& associationPath) :
    tid(tid),
    sensorId(pdr->sensor_id), sensorName(sensorName), isPriority(false)
{
    std::string path;
    SensorUnit sensorUnit = SensorUnit::DegreesC;

    switch (pdr->base_unit)
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
        default:
            lg2::error("Sensor {NAME} has Invalid baseUnit {UNIT}.", "NAME",
                       sensorName, "UNIT", pdr->base_unit);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
            break;
    }

    path = sensorNameSpace + sensorName;
    path = std::regex_replace(path, std::regex("[^a-zA-Z0-9_/]+"), "_");

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
        {{"chassis", "all_sensors", associationPath.c_str()}});

    double maxValue = std::numeric_limits<double>::quiet_NaN();
    double minValue = std::numeric_limits<double>::quiet_NaN();

    switch (pdr->sensor_data_size)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
            maxValue = pdr->max_readable.value_u8;
            minValue = pdr->min_readable.value_u8;
            hysteresis = pdr->hysteresis.value_u8;
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            maxValue = pdr->max_readable.value_s8;
            minValue = pdr->min_readable.value_s8;
            hysteresis = pdr->hysteresis.value_s8;
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
            maxValue = pdr->max_readable.value_u16;
            minValue = pdr->min_readable.value_u16;
            hysteresis = pdr->hysteresis.value_u16;
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            maxValue = pdr->max_readable.value_s16;
            minValue = pdr->min_readable.value_s16;
            hysteresis = pdr->hysteresis.value_s16;
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
            maxValue = pdr->max_readable.value_u32;
            minValue = pdr->min_readable.value_u32;
            hysteresis = pdr->hysteresis.value_u32;
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            maxValue = pdr->max_readable.value_s32;
            minValue = pdr->min_readable.value_s32;
            hysteresis = pdr->hysteresis.value_s32;
            break;
    }

    bool hasCriticalThresholds = false;
    bool hasWarningThresholds = false;
    double criticalHigh = std::numeric_limits<double>::quiet_NaN();
    double criticalLow = std::numeric_limits<double>::quiet_NaN();
    double warningHigh = std::numeric_limits<double>::quiet_NaN();
    double warningLow = std::numeric_limits<double>::quiet_NaN();

    if (pdr->supported_thresholds.bits.bit0)
    {
        hasWarningThresholds = true;
        switch (pdr->range_field_format)
        {
            case PLDM_RANGE_FIELD_FORMAT_UINT8:
                warningHigh = pdr->warning_high.value_u8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT8:
                warningHigh = pdr->warning_high.value_s8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT16:
                warningHigh = pdr->warning_high.value_u16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT16:
                warningHigh = pdr->warning_high.value_s16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT32:
                warningHigh = pdr->warning_high.value_u32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT32:
                warningHigh = pdr->warning_high.value_s32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_REAL32:
                warningHigh = pdr->warning_high.value_f32;
                break;
        }
    }

    if (pdr->supported_thresholds.bits.bit3)
    {
        hasWarningThresholds = true;
        switch (pdr->range_field_format)
        {
            case PLDM_RANGE_FIELD_FORMAT_UINT8:
                warningLow = pdr->warning_low.value_u8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT8:
                warningLow = pdr->warning_low.value_s8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT16:
                warningLow = pdr->warning_low.value_u16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT16:
                warningLow = pdr->warning_low.value_s16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT32:
                warningLow = pdr->warning_low.value_u32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT32:
                warningLow = pdr->warning_low.value_s32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_REAL32:
                warningLow = pdr->warning_low.value_f32;
                break;
        }
    }

    if (pdr->supported_thresholds.bits.bit1)
    {
        hasCriticalThresholds = true;
        switch (pdr->range_field_format)
        {
            case PLDM_RANGE_FIELD_FORMAT_UINT8:
                criticalHigh = pdr->critical_high.value_u8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT8:
                criticalHigh = pdr->critical_high.value_s8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT16:
                criticalHigh = pdr->critical_high.value_u16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT16:
                criticalHigh = pdr->critical_high.value_s16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT32:
                criticalHigh = pdr->critical_high.value_u32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT32:
                criticalHigh = pdr->critical_high.value_s32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_REAL32:
                criticalHigh = pdr->critical_high.value_f32;
                break;
        }
    }

    if (pdr->supported_thresholds.bits.bit4)
    {
        hasCriticalThresholds = true;
        switch (pdr->range_field_format)
        {
            case PLDM_RANGE_FIELD_FORMAT_UINT8:
                criticalLow = pdr->critical_low.value_u8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT8:
                criticalLow = pdr->critical_low.value_s8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT16:
                criticalLow = pdr->critical_low.value_u16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT16:
                criticalLow = pdr->critical_low.value_s16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT32:
                criticalLow = pdr->critical_low.value_u32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT32:
                criticalLow = pdr->critical_low.value_s32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_REAL32:
                criticalLow = pdr->critical_low.value_f32;
                break;
        }
    }

    resolution = pdr->resolution;
    offset = pdr->offset;
    baseUnitModifier = pdr->unit_modifier;

    timeStamp = 0;
    updateTime = (uint64_t)DEFAULT_SENSOR_UPDATER_INTERVAL * 1000;
    if (!std::isnan(pdr->update_interval))
    {
        updateTime = pdr->update_interval * 1000000;
    }

    try
    {
        valueIntf = std::make_unique<ValueIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Value interface for numeric sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    valueIntf->maxValue(unitModifier(conversionFormula(maxValue)));
    valueIntf->minValue(unitModifier(conversionFormula(minValue)));
    hysteresis = unitModifier(conversionFormula(hysteresis));
    valueIntf->unit(sensorUnit);

    try
    {
        availabilityIntf = std::make_unique<AvailabilityIntf>(bus,
                                                              path.c_str());
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

    if (hasWarningThresholds)
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

    if (hasCriticalThresholds)
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
}

NumericSensor::NumericSensor(
    const pldm_tid_t tid, const bool sensorDisabled,
    std::shared_ptr<pldm_compact_numeric_sensor_pdr> pdr,
    std::string& sensorName, std::string& associationPath) :
    tid(tid),
    sensorId(pdr->sensor_id), sensorName(sensorName), isPriority(false)
{
    std::string path;
    SensorUnit sensorUnit = SensorUnit::DegreesC;

    switch (pdr->base_unit)
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
        default:
            lg2::error("Sensor {NAME} has Invalid baseUnit {UNIT}.", "NAME",
                       sensorName, "UNIT", pdr->base_unit);
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InvalidArgument();
            break;
    }

    path = sensorNameSpace + sensorName;
    path = std::regex_replace(path, std::regex("[^a-zA-Z0-9_/]+"), "_");

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
    double criticalHigh = std::numeric_limits<double>::quiet_NaN();
    double criticalLow = std::numeric_limits<double>::quiet_NaN();
    double warningHigh = std::numeric_limits<double>::quiet_NaN();
    double warningLow = std::numeric_limits<double>::quiet_NaN();

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

    resolution = std::numeric_limits<double>::quiet_NaN();
    offset = std::numeric_limits<double>::quiet_NaN();
    baseUnitModifier = pdr->unit_modifier;

    timeStamp = 0;
    updateTime = (uint64_t)DEFAULT_SENSOR_UPDATER_INTERVAL * 1000;
    try
    {
        valueIntf = std::make_unique<ValueIntf>(bus, path.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Value interface for compact numeric sensor {PATH} error - {ERROR}",
            "PATH", path, "ERROR", e);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }
    valueIntf->maxValue(unitModifier(conversionFormula(maxValue)));
    valueIntf->minValue(unitModifier(conversionFormula(minValue)));
    hysteresis = unitModifier(conversionFormula(hysteresis));
    valueIntf->unit(sensorUnit);

    try
    {
        availabilityIntf = std::make_unique<AvailabilityIntf>(bus,
                                                              path.c_str());
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

    if (hasWarningThresholds)
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

    if (hasCriticalThresholds)
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
}

double NumericSensor::conversionFormula(double value)
{
    double convertedValue = value;
    convertedValue *= std::isnan(resolution) ? 1 : resolution;
    convertedValue += std::isnan(offset) ? 0 : offset;
    return convertedValue;
}

double NumericSensor::unitModifier(double value)
{
    return std::isnan(value) ? value : value * std::pow(10, baseUnitModifier);
}

void NumericSensor::updateReading(bool available, bool functional, double value)
{
    availabilityIntf->available(available);
    operationalStatusIntf->functional(functional);
    double curValue = valueIntf->value();
    double newValue = std::numeric_limits<double>::quiet_NaN();
    if (functional && available)
    {
        newValue = unitModifier(conversionFormula(value));
        if (curValue != newValue)
        {
            valueIntf->value(unitModifier(conversionFormula(value)));
            updateThresholds();
        }
    }
    else
    {
        if (curValue != newValue)
        {
            valueIntf->value(std::numeric_limits<double>::quiet_NaN());
        }
    }
}

void NumericSensor::handleErrGetSensorReading()
{
    operationalStatusIntf->functional(false);
    valueIntf->value(std::numeric_limits<double>::quiet_NaN());
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
        else if (value < (threshold - hyst))
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
        else if (value > (threshold + hyst))
        {
            return false;
        }
    }
    return alarm;
}

void NumericSensor::updateThresholds()
{
    auto value = valueIntf->value();

    if (thresholdWarningIntf &&
        !std::isnan(thresholdWarningIntf->warningHigh()))
    {
        auto threshold = thresholdWarningIntf->warningHigh();
        auto alarm = thresholdWarningIntf->warningAlarmHigh();
        auto newAlarm = checkThreshold(alarm, true, value, threshold,
                                       hysteresis);
        if (alarm != newAlarm)
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
    }

    if (thresholdWarningIntf && !std::isnan(thresholdWarningIntf->warningLow()))
    {
        auto threshold = thresholdWarningIntf->warningLow();
        auto alarm = thresholdWarningIntf->warningAlarmLow();
        auto newAlarm = checkThreshold(alarm, false, value, threshold,
                                       hysteresis);
        if (alarm != newAlarm)
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

    if (thresholdCriticalIntf &&
        !std::isnan(thresholdCriticalIntf->criticalHigh()))
    {
        auto threshold = thresholdCriticalIntf->criticalHigh();
        auto alarm = thresholdCriticalIntf->criticalAlarmHigh();
        auto newAlarm = checkThreshold(alarm, true, value, threshold,
                                       hysteresis);
        if (alarm != newAlarm)
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
    }

    if (thresholdCriticalIntf &&
        !std::isnan(thresholdCriticalIntf->criticalLow()))
    {
        auto threshold = thresholdCriticalIntf->criticalLow();
        auto alarm = thresholdCriticalIntf->criticalAlarmLow();
        auto newAlarm = checkThreshold(alarm, false, value, threshold,
                                       hysteresis);
        if (alarm != newAlarm)
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
}
} // namespace platform_mc
} // namespace pldm
