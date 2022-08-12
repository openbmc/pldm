#include "compact_numeric_sensor.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"

#include <limits>
#include <regex>

namespace pldm
{
namespace platform_mc
{

CompactNumericSensor::CompactNumericSensor(
    const uint8_t tid, const bool sensorDisabled,
    std::shared_ptr<pldm_compact_numeric_sensor_pdr> pdr,
    std::string& sensorName, std::string& associationPath) :
    tid(tid),
    sensorId(pdr->sensor_id), pdr(pdr)
{
    std::string path;
    SensorUnit sensorUnit = SensorUnit::DegreesC;

    switch (pdr->base_unit)
    {
        case PLDM_SENSOR_UNIT_DEGRESS_C:
            path = "/xyz/openbmc_project/sensors/temperature/";
            sensorUnit = SensorUnit::DegreesC;
            break;
        case PLDM_SENSOR_UNIT_VOLTS:
            path = "/xyz/openbmc_project/sensors/voltage/";
            sensorUnit = SensorUnit::Volts;
            break;
        case PLDM_SENSOR_UNIT_AMPS:
            path = "/xyz/openbmc_project/sensors/current/";
            sensorUnit = SensorUnit::Amperes;
            break;
        case PLDM_SENSOR_UNIT_RPM:
            path = "/xyz/openbmc_project/sensors/fan_pwm/";
            sensorUnit = SensorUnit::RPMS;
            break;
        case PLDM_SENSOR_UNIT_WATTS:
            path = "/xyz/openbmc_project/sensors/power/";
            sensorUnit = SensorUnit::Watts;
            break;
        case PLDM_SENSOR_UNIT_JOULES:
            path = "/xyz/openbmc_project/sensors/energy/";
            sensorUnit = SensorUnit::Joules;
            break;
        case PLDM_SENSOR_UNIT_PERCENTAGE:
            path = "/xyz/openbmc_project/sensors/utilization/";
            sensorUnit = SensorUnit::Percent;
            break;
        default:
            throw std::runtime_error("baseUnit(" +
                                     std::to_string(pdr->base_unit) +
                                     ") is not of supported type");
            break;
    }

    path += sensorName;
    path = std::regex_replace(path, std::regex("[^a-zA-Z0-9_/]+"), "_");

    auto& bus = pldm::utils::DBusHandler::getBus();
    associationDefinitionsIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    associationDefinitionsIntf->associations(
        {{"chassis", "all_sensors", associationPath.c_str()}});

    /* there is no maxValue, minValue, hysteresis in compact numeric PDRs */
    double maxValue = std::numeric_limits<double>::quiet_NaN();
    double minValue = std::numeric_limits<double>::quiet_NaN();

    bool hasCriticalThresholds = false;
    bool hasWarningThresholds = false;
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
        criticalHigh = pdr->critical_high;
    }

    elapsedTime = 0;
    updateTime = 1000000;
    switch (pdr->occurrence_rate)
    {
        case PLDM_RATE_UNIT_NONE:
        case PLDM_RATE_UNIT_PER_MICRO_SECOND:
            std::cerr << "occurrence_rate per micro second too small."
                      << "Apply per second rate.\n";
        case PLDM_RATE_UNIT_PER_MILLI_SECOND:
            std::cerr << "occurrence_rate per milli second too small."
                      << "Apply per second rate.\n";
        case PLDM_RATE_UNIT_PER_SECOND:
            updateTime = 1000000;
            break;
        case PLDM_RATE_UNIT_PER_MINUTE:
            updateTime = 60000000;
            break;
        case PLDM_RATE_UNIT_PER_HOUR:
            updateTime = 3600000000;
            break;
        case PLDM_RATE_UNIT_PER_DAY:
            updateTime = 86400000000;
            break;
        default:
            std::cerr << "occurrence_rate("
                      << std::to_string(pdr->occurrence_rate)
                      << ") is not of supported type\n";
            break;
    }

    valueIntf = std::make_unique<ValueIntf>(bus, path.c_str());
    valueIntf->maxValue(unitModifier(conversionFormula(maxValue)));
    valueIntf->minValue(unitModifier(conversionFormula(minValue)));
    hysteresis = unitModifier(conversionFormula(hysteresis));
    valueIntf->unit(sensorUnit);

    availabilityIntf = std::make_unique<AvailabilityIntf>(bus, path.c_str());
    availabilityIntf->available(true);

    operationalStatusIntf =
        std::make_unique<OperationalStatusIntf>(bus, path.c_str());
    operationalStatusIntf->functional(!sensorDisabled);

    if (hasWarningThresholds)
    {
        thresholdWarningIntf =
            std::make_unique<ThresholdWarningIntf>(bus, path.c_str());
        thresholdWarningIntf->warningHigh(unitModifier(warningHigh));
        thresholdWarningIntf->warningLow(unitModifier(warningLow));
    }

    if (hasCriticalThresholds)
    {
        thresholdCriticalIntf =
            std::make_unique<ThresholdCriticalIntf>(bus, path.c_str());
        thresholdCriticalIntf->criticalHigh(unitModifier(criticalHigh));
        thresholdCriticalIntf->criticalLow(unitModifier(criticalLow));
    }
}

double CompactNumericSensor::conversionFormula(double value)
{
    double convertedValue = value;
    convertedValue *= std::isnan(resolution) ? 1 : resolution;
    convertedValue += std::isnan(offset) ? 0 : offset;

    return convertedValue;
}

double CompactNumericSensor::unitModifier(double value)
{
    return value * std::pow(10, baseUnitModifier);
}

void CompactNumericSensor::updateReading(bool available, bool functional,
                                         double value)
{
    availabilityIntf->available(available);
    operationalStatusIntf->functional(functional);
    if (functional && available)
    {
        valueIntf->value(unitModifier(conversionFormula(value)));
        updateThresholds();
    }
    else
    {
        valueIntf->value(std::numeric_limits<double>::quiet_NaN());
    }
}

void CompactNumericSensor::handleErrGetSensorReading()
{
    operationalStatusIntf->functional(false);
    valueIntf->value(std::numeric_limits<double>::quiet_NaN());
}

bool CompactNumericSensor::checkThreshold(bool alarm, bool direction,
                                          double value, double threshold,
                                          double hyst)
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

void CompactNumericSensor::updateThresholds()
{
    auto value = valueIntf->value();
    if (thresholdWarningIntf)
    {
        auto threshold = thresholdWarningIntf->warningHigh();
        auto alarm = thresholdWarningIntf->warningAlarmHigh();
        auto newAlarm =
            checkThreshold(alarm, true, value, threshold, hysteresis);
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

        threshold = thresholdWarningIntf->warningLow();
        alarm = thresholdWarningIntf->warningAlarmLow();
        newAlarm = checkThreshold(alarm, false, value, threshold, hysteresis);
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
        auto newAlarm =
            checkThreshold(alarm, true, value, threshold, hysteresis);
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
        auto newAlarm =
            checkThreshold(alarm, false, value, threshold, hysteresis);
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
