#include "numeric_effecter_dbus.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"

#include <limits>
#include <regex>

namespace pldm
{
namespace platform_mc
{

NumericEffecterDbus::NumericEffecterDbus(
    const uint8_t tid, const bool sensorDisabled,
    std::shared_ptr<pldm_numeric_effecter_value_pdr> pdr,
    std::string& sensorName, std::string& associationPath) :
    tid(tid),
    sensorId(pdr->effecter_id), pdr(pdr)
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

    resolution = pdr->resolution;
    offset = pdr->offset;
    baseUnitModifier = pdr->unit_modifier;

    double maxValue = std::numeric_limits<double>::quiet_NaN();
    double minValue = std::numeric_limits<double>::quiet_NaN();

    if (pdr->range_field_support.bits.bit1)
    {
        switch (pdr->range_field_format)
        {
            case PLDM_RANGE_FIELD_FORMAT_UINT8:
                maxValue = pdr->normal_max.value_u8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT8:
                maxValue = pdr->normal_max.value_s8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT16:
                maxValue = pdr->normal_max.value_u16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT16:
                maxValue = pdr->normal_max.value_s16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT32:
                maxValue = pdr->normal_max.value_u32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT32:
                maxValue = pdr->normal_max.value_s32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_REAL32:
                maxValue = pdr->normal_max.value_f32;
                break;
        }
    }

    if (pdr->range_field_support.bits.bit2)
    {
        switch (pdr->range_field_format)
        {
            case PLDM_RANGE_FIELD_FORMAT_UINT8:
                minValue = pdr->normal_min.value_u8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT8:
                minValue = pdr->normal_min.value_s8;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT16:
                minValue = pdr->normal_min.value_u16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT16:
                minValue = pdr->normal_min.value_s16;
                break;
            case PLDM_RANGE_FIELD_FORMAT_UINT32:
                minValue = pdr->normal_min.value_u32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_SINT32:
                minValue = pdr->normal_min.value_s32;
                break;
            case PLDM_RANGE_FIELD_FORMAT_REAL32:
                minValue = pdr->normal_min.value_f32;
                break;
        }
    }

    elapsedTime = 0;
    switch (pdr->rate_unit)
    {
        case PLDM_RATE_UNIT_NONE:
        case PLDM_RATE_UNIT_PER_SECOND:
            updateTime = 1000000;
            break;
        case PLDM_RATE_UNIT_PER_MICRO_SECOND:
            std::cerr << "occurrence_rate per micro second too small."
                      << "Apply per second rate.\n";
            updateTime = 1000000;
            break;
        case PLDM_RATE_UNIT_PER_MILLI_SECOND:
            std::cerr << "occurrence_rate per milli second too small."
                      << "Apply per second rate.\n";
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
            std::cerr << "occurrence_rate(" << std::to_string(pdr->rate_unit)
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
}

double NumericEffecterDbus::conversionFormula(double value)
{
    double convertedValue = value;
    convertedValue *= std::isnan(resolution) ? 1 : resolution;
    convertedValue += std::isnan(offset) ? 0 : offset;

    return convertedValue;
}

double NumericEffecterDbus::unitModifier(double value)
{
    return value * std::pow(10, baseUnitModifier);
}

void NumericEffecterDbus::updateReading(bool available, bool functional,
                                        double value)
{
    availabilityIntf->available(available);
    operationalStatusIntf->functional(functional);
    if (functional && available)
    {
        valueIntf->value(unitModifier(conversionFormula(value)));
    }
    else
    {
        valueIntf->value(std::numeric_limits<double>::quiet_NaN());
    }
}

void NumericEffecterDbus::handleErrGetSensorReading()
{
    operationalStatusIntf->functional(false);
    valueIntf->value(std::numeric_limits<double>::quiet_NaN());
}

bool NumericEffecterDbus::checkThreshold(bool alarm, bool direction,
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

} // namespace platform_mc
} // namespace pldm
