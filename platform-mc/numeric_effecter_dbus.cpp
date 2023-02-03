#include "numeric_effecter_dbus.hpp"

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

NumericEffecterDbus::NumericEffecterDbus(const tid_t tId,
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

    auto pdr = reinterpret_cast<const pldm_numeric_effecter_value_pdr*>(
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
    /* default refresh interval of effecter is 1 seconds*/
    updateTime = 1000000;

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

std::vector<uint8_t> NumericEffecterDbus::getReadingRequest()
{
    std::vector<uint8_t> request;
    request.resize(sizeof(pldm_msg_hdr) +
                   PLDM_GET_NUMERIC_EFFECTER_VALUE_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_get_numeric_effecter_value_req(0, sensorId, requestMsg);
    if (rc)
    {
        std::cerr << "encode_get_sensor_reading_req failed, TID="
                  << unsigned(tid) << ", RC=" << rc << std::endl;
        return {};
    }

    return request;
}

int NumericEffecterDbus::handleReadingResponse(const pldm_msg* responseMsg,
                                               size_t responseLen)
{
    if (!responseMsg)
    {
        return PLDM_ERROR;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint8_t sensorDataSize = PLDM_SENSOR_DATA_SIZE_SINT32;
    uint8_t sensorOperationalState = 0;
    union_range_field_format presentReading;
    union_range_field_format pendingValue;
    auto rc = decode_get_numeric_effecter_value_resp(
        responseMsg, responseLen, &completionCode, &sensorDataSize,
        &sensorOperationalState, reinterpret_cast<uint8_t*>(&pendingValue),
        reinterpret_cast<uint8_t*>(&presentReading));
    if (rc)
    {
        std::cerr << "Failed to decode response of GetSensorReading, TID="
                  << unsigned(tid) << ", RC=" << rc << "\n";
        handleErrGetSensorReading();
        return rc;
    }

    if (completionCode != PLDM_SUCCESS)
    {
        std::cerr << "Failed to decode response of GetSensorReading, TID="
                  << unsigned(tid) << ", CC=" << unsigned(completionCode)
                  << "\n";
        return completionCode;
    }

    switch (sensorOperationalState)
    {
        case EFFECTER_OPER_STATE_ENABLED_UPDATEPENDING:
        case EFFECTER_OPER_STATE_ENABLED_NOUPDATEPENDING:
            break;
        case EFFECTER_OPER_STATE_DISABLED:
            updateReading(true, false, 0);
            return completionCode;
        case EFFECTER_OPER_STATE_UNAVAILABLE:
        default:
            updateReading(false, false, 0);
            return completionCode;
    }

    double value = std::numeric_limits<double>::quiet_NaN();
    switch (sensorDataSize)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
            value = static_cast<double>(presentReading.value_u8);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            value = static_cast<double>(presentReading.value_s8);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
            value = static_cast<double>(presentReading.value_u16);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            value = static_cast<double>(presentReading.value_s16);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
            value = static_cast<double>(presentReading.value_u32);
            break;
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            value = static_cast<double>(presentReading.value_s32);
            break;
        default:
            value = std::numeric_limits<double>::quiet_NaN();
            break;
    }

    updateReading(true, true, value);

    return completionCode;
}

} // namespace platform_mc
} // namespace pldm
