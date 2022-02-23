#include "numeric_sensor.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"

#include <math.h>

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
    switch (pdr->base_unit)
    {
        case PLDM_SENSOR_UNIT_DEGRESS_C:
            path = "/xyz/openbmc_project/sensors/temperature/";
            break;
        case PLDM_SENSOR_UNIT_VOLTS:
            path = "/xyz/openbmc_project/sensors/voltage/";
            break;
        case PLDM_SENSOR_UNIT_AMPS:
            path = "/xyz/openbmc_project/sensors/current/";
            break;
        case PLDM_SENSOR_UNIT_RPM:
            path = "/xyz/openbmc_project/sensors/fan_pwm/";
            break;
        case PLDM_SENSOR_UNIT_WATTS:
            path = "/xyz/openbmc_project/sensors/power/";
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
        std::make_shared<AssociationDefinitionsInft>(bus, path.c_str());
    associationDefinitionsIntf->associations(
        {{"chassis", "all_sensors", associationPath.c_str()}});

    switch (pdr->sensor_data_size)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
            maxValue = pdr->max_readable.value_u8;
            minValue = pdr->min_readable.value_u8;
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            maxValue = pdr->max_readable.value_s8;
            minValue = pdr->min_readable.value_s8;
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
            maxValue = pdr->max_readable.value_u16;
            minValue = pdr->min_readable.value_u16;
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            maxValue = pdr->max_readable.value_s16;
            minValue = pdr->min_readable.value_s16;
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
            maxValue = pdr->max_readable.value_u32;
            minValue = pdr->min_readable.value_u32;
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            maxValue = pdr->max_readable.value_s32;
            minValue = pdr->min_readable.value_s32;
            break;
    }

    elapsedTime = 0;
    updateTime = ceil(pdr->update_interval);

    valueIntf = std::make_shared<ValueIntf>(bus, path.c_str());
    valueIntf->maxValue(maxValue);
    valueIntf->minValue(minValue);

    availabilityIntf = std::make_shared<AvailabilityIntf>(bus, path.c_str());
    availabilityIntf->available(true);

    operationalStatusIntf =
        std::make_shared<OperationalStatusIntf>(bus, path.c_str());
    operationalStatusIntf->functional(!sensorDisabled);
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

} // namespace platform_mc
} // namespace pldm
