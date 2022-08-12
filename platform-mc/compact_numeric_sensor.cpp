#include "compact_numeric_sensor.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"
#include "requester/handler.hpp"
#include "sensor_intf.hpp"

#include <limits>
#include <regex>

namespace pldm
{
namespace platform_mc
{

CompactNumericSensor::CompactNumericSensor(const tid_t tId,
                                           const bool sensorDisabled,
                                           const std::vector<uint8_t>& pdrData,
                                           uint16_t sensorId,
                                           std::string& sensorName,
                                           std::string& associationPath) :
    SensorIntf(tId, sensorDisabled, pdrData, sensorId, sensorName,
               associationPath)
{
    std::string path;
    SensorUnit sensorUnit = SensorUnit::DegreesC;

    auto pdr = reinterpret_cast<const pldm_compact_numeric_sensor_pdr*>(
        pdrData.data());

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

    baseUnitModifier = pdr->unit_modifier;

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
        case PLDM_RATE_UNIT_PER_MILLI_SECOND:
        case PLDM_RATE_UNIT_PER_SECOND:
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

} // namespace platform_mc
} // namespace pldm
