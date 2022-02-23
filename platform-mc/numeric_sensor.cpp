#include "numeric_sensor.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"

#include <limits>
#include <regex>

namespace pldm
{
namespace platform_mc
{

NumericSensor::NumericSensor(const uint8_t eid, const uint8_t tid,
                             const bool sensorDisabled,
                             std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr,
                             std::string& sensorName,
                             std::string& associationPath) :
    eid(eid),
    tid(tid), sensorId(pdr->sensor_id), pdr(pdr)
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
        default:
            throw std::runtime_error("baseUnit(" +
                                     std::to_string(pdr->base_unit) +
                                     ") is not of supported type");
            break;
    }

    path += sensorName;
    std::regex_replace(path, std::regex("[^a-zA-Z0-9_/]+"), "_");

    auto& bus = pldm::utils::DBusHandler::getBus();
    associationDefinitionsIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    associationDefinitionsIntf->associations(
        {{"chassis", "all_sensors", associationPath.c_str()}});

    double maxValue = 0;
    double minValue = 0;
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
    double criticalHigh = std::numeric_limits<double>::quiet_NaN();
    double criticalLow = std::numeric_limits<double>::quiet_NaN();
    double warningHigh = std::numeric_limits<double>::quiet_NaN();
    double warningLow = std::numeric_limits<double>::quiet_NaN();

    switch (pdr->range_field_format)
    {
        case PLDM_RANGE_FIELD_FORMAT_UINT8:
            warningHigh = pdr->warning_high.value_u8;
            warningLow = pdr->warning_low.value_u8;
            break;
        case PLDM_RANGE_FIELD_FORMAT_SINT8:
            warningHigh = pdr->warning_high.value_s8;
            warningLow = pdr->warning_low.value_u8;
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT16:
            warningHigh = pdr->warning_high.value_u16;
            warningLow = pdr->warning_low.value_u16;
            break;
        case PLDM_RANGE_FIELD_FORMAT_SINT16:
            warningHigh = pdr->warning_high.value_s16;
            warningLow = pdr->warning_low.value_s16;
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT32:
            warningHigh = pdr->warning_high.value_u32;
            warningLow = pdr->warning_low.value_u32;
            break;
        case PLDM_RANGE_FIELD_FORMAT_SINT32:
            warningHigh = pdr->warning_high.value_s32;
            warningLow = pdr->warning_low.value_s32;
            break;
        case PLDM_RANGE_FIELD_FORMAT_REAL32:
            warningHigh = pdr->warning_high.value_f32;
            warningLow = pdr->warning_low.value_f32;
            break;
    }

    if (pdr->range_field_support.bits.bit3)
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

    if (pdr->range_field_support.bits.bit4)
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

    elapsedTime = 0;
    updateTime = pdr->update_interval * 10000000;

    valueIntf = std::make_unique<ValueIntf>(bus, path.c_str());
    valueIntf->maxValue(maxValue);
    valueIntf->minValue(minValue);
    valueIntf->unit(sensorUnit);

    availabilityIntf = std::make_unique<AvailabilityIntf>(bus, path.c_str());
    availabilityIntf->available(true);

    operationalStatusIntf =
        std::make_unique<OperationalStatusIntf>(bus, path.c_str());
    operationalStatusIntf->functional(!sensorDisabled);

    thresholdWarningIntf =
        std::make_unique<ThresholdWarningIntf>(bus, path.c_str());
    thresholdWarningIntf->warningHigh(warningHigh);
    thresholdWarningIntf->warningLow(warningLow);

    if (hasCriticalThresholds)
    {
        thresholdCriticalIntf =
            std::make_unique<ThresholdCriticalIntf>(bus, path.c_str());
        thresholdCriticalIntf->criticalHigh(criticalHigh);
        thresholdCriticalIntf->criticalLow(criticalLow);
    }
}

void NumericSensor::updateReading(bool available, bool functional, double value)
{
    availabilityIntf->available(available);
    operationalStatusIntf->functional(functional);
    if (functional && available)
    {
        double convertedValue = value;
        convertedValue *= pdr->resolution;
        convertedValue += pdr->offset;
        convertedValue *= std::pow(10, pdr->unit_modifier);
        valueIntf->value(convertedValue);
        updateThresholds();
    }
    else
    {
        valueIntf->value(std::numeric_limits<double>::quiet_NaN());
    }
}

void NumericSensor::handleErrGetSensorReading()
{
    operationalStatusIntf->functional(false);
    valueIntf->value(std::numeric_limits<double>::quiet_NaN());
}

void NumericSensor::updateThresholds()
{
    auto value = valueIntf->value();
    if (thresholdWarningIntf)
    {
        if (value >= thresholdWarningIntf->warningHigh())
        {
            if (!thresholdWarningIntf->warningAlarmHigh())
            {
                thresholdWarningIntf->warningAlarmHigh(true);
                thresholdWarningIntf->warningHighAlarmAsserted(value);
            }
        }
        else if (value < (thresholdWarningIntf->warningHigh() - hysteresis))
        {
            if (thresholdWarningIntf->warningAlarmHigh())
            {
                thresholdWarningIntf->warningAlarmHigh(false);
                thresholdWarningIntf->warningHighAlarmDeasserted(value);
            }
        }

        if (value <= thresholdWarningIntf->warningLow())
        {
            if (!thresholdWarningIntf->warningAlarmLow())
            {
                thresholdWarningIntf->warningAlarmLow(true);
                thresholdWarningIntf->warningLowAlarmAsserted(value);
            }
        }
        else if (value < (thresholdWarningIntf->warningLow() + hysteresis))
        {
            if (thresholdWarningIntf->warningAlarmLow())
            {
                thresholdWarningIntf->warningAlarmLow(false);
                thresholdWarningIntf->warningLowAlarmDeasserted(value);
            }
        }
    }

    if (thresholdCriticalIntf && thresholdCriticalIntf->criticalHigh() !=
                                     std::numeric_limits<double>::quiet_NaN())
    {
        if (value >= thresholdCriticalIntf->criticalHigh())
        {
            if (!thresholdCriticalIntf->criticalAlarmHigh())
            {
                thresholdCriticalIntf->criticalAlarmHigh(true);
                thresholdCriticalIntf->criticalHighAlarmAsserted(value);
            }
        }
        else if (value < (thresholdCriticalIntf->criticalHigh() - hysteresis))
        {
            if (thresholdCriticalIntf->criticalAlarmHigh())
            {
                thresholdCriticalIntf->criticalAlarmHigh(false);
                thresholdCriticalIntf->criticalHighAlarmDeasserted(value);
            }
        }
    }

    if (thresholdCriticalIntf && thresholdCriticalIntf->criticalLow() !=
                                     std::numeric_limits<double>::quiet_NaN())
    {
        if (value <= thresholdCriticalIntf->criticalLow())
        {
            if (!thresholdCriticalIntf->criticalAlarmLow())
            {
                thresholdCriticalIntf->criticalAlarmLow(true);
                thresholdCriticalIntf->criticalLowAlarmAsserted(value);
            }
        }
        else if (value < (thresholdCriticalIntf->criticalLow() + hysteresis))
        {
            if (thresholdCriticalIntf->criticalAlarmLow())
            {
                thresholdCriticalIntf->criticalAlarmLow(false);
                thresholdCriticalIntf->criticalLowAlarmDeasserted(value);
            }
        }
    }
}
} // namespace platform_mc
} // namespace pldm
