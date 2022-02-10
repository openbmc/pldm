#include "numeric_sensor.hpp"
#include "libpldm/platform.h"
#include <regex>

namespace pldm
{
namespace platform_mc
{

NumericSensor::NumericSensor(const uint8_t eid, const uint8_t tid,
                             const bool sensorDisabled,
                             const NumericSensorInfo& info,
                             std::string& associationPath) :
    eid(eid),
    tid(tid), sensorId(info.sensorId), maxValue(info.max), minValue(info.min),
    resolution(info.resolution), offset(info.offset),
    unitModifier(info.unitModifier)
{
    std::string path;
    switch (info.baseUnit)
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
                                     std::to_string(info.baseUnit) +
                                     ") is not of supported type");
            break;
    }

    if (info.sensorName.empty())
    {
        path = path + "PLDM_Device_" + std::to_string(info.sensorId) + "_" +
               std::to_string(tid);
    }
    else
    {
        path = path + std::regex_replace(info.sensorName,
                                         std::regex("[^a-zA-Z0-9_/]+"), "_");
    }

    auto& bus = pldm::utils::DBusHandler::getBus();
    associationDefinitionsIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, path.c_str());
    associationDefinitionsIntf->associations(
        {{"chassis", "all_sensors", associationPath.c_str()}});

    valueIntf = std::make_shared<ValueIntf>(bus, path.c_str());
    valueIntf->maxValue(maxValue);
    valueIntf->minValue(minValue);

    availabilityIntf = std::make_shared<AvailabilityIntf>(bus, path.c_str());
    availabilityIntf->available(true);

    operationalStatusIntf =
        std::make_shared<OperationalStatusIntf>(bus, path.c_str());
    operationalStatusIntf->functional(!sensorDisabled);
}

void NumericSensor::handleRespGetSensorReading(mctp_eid_t eid,
                                               const pldm_msg* response,
                                               size_t respMsgLen)
{
    if (response == nullptr || !respMsgLen)
    {
        std::cerr << "No response received for GetSensorReading, EID="
                  << unsigned(eid) << "\n";
        handleErrGetSensorReading();
        return;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint8_t sensorDataSize;
    uint8_t sensorOperationalState;
    uint8_t sensorEventMessageEnable;
    uint8_t presentState;
    uint8_t previousState;
    uint8_t eventState;
    union_sensor_data_size presentReading;
    auto rc = decode_get_sensor_reading_resp(
        response, respMsgLen, &completionCode, &sensorDataSize,
        &sensorOperationalState, &sensorEventMessageEnable, &presentState,
        &previousState, &eventState,
        reinterpret_cast<uint8_t*>(&presentReading));
    if (rc)
    {
        std::cerr << "Failed to decode response of GetSensorReading, EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        handleErrGetSensorReading();
        return;
    }

    switch (sensorOperationalState)
    {
        case PLDM_SENSOR_DISABLED:
            updateReading(true, false);
            return;
        case PLDM_SENSOR_UNAVAILABLE:
            updateReading(false, false);
            return;
        case PLDM_SENSOR_ENABLED:
            break;
        default:
            return;
    }

    double value;
    switch (sensorDataSize)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
            value = static_cast<float>(presentReading.value_u8);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            value = static_cast<float>(presentReading.value_s8);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
            value = static_cast<float>(presentReading.value_u16);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            value = static_cast<float>(presentReading.value_s16);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
            value = static_cast<float>(presentReading.value_u32);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            value = static_cast<float>(presentReading.value_s32);
            break;
        default:
            value = 0;
            break;
    }

    updateReading(true, true, value);
}

void NumericSensor::updateReading(bool available, bool functional, double value)
{
    availabilityIntf->available(available);
    operationalStatusIntf->functional(functional);
    if (functional && available)
    {
        double convertedValue = value;
        convertedValue = resolution * convertedValue + offset;
        convertedValue = convertedValue * std::pow(10, unitModifier);
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