#include "sensor_intf.hpp"

#include "libpldm/platform.h"

#include "common/utils.hpp"

#include <limits>
#include <regex>

namespace pldm
{
namespace platform_mc
{

/** @brief ConversionFormula is used to convert raw value to the units
 * specified in the PDR
 *
 *
 *  @param[in] value - raw value
 *  @return double - converted value
 */
double SensorIntf::conversionFormula(double value)
{
    double convertedValue = value;
    convertedValue *= std::isnan(resolution) ? 1 : resolution;
    convertedValue += std::isnan(offset) ? 0 : offset;

    return convertedValue;
}

/** @brief UnitModifier is used to apply the unit modifier specified in PDR
 *
 *  @param[in] value - raw value
 *  @return double - converted value
 */
double SensorIntf::unitModifier(double value)
{
    return value * std::pow(10, baseUnitModifier);
}

/** @brief The function called by Sensor Manager to set sensor to
 * error status.
 */
void SensorIntf::handleErrGetSensorReading()
{
    operationalStatusIntf->functional(false);
    valueIntf->value(std::numeric_limits<double>::quiet_NaN());
}

/** @brief Updating the sensor status to D-Bus interface
 */
void SensorIntf::updateReading(bool available, bool functional, double value)
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

/** @brief Check if value is over threshold.
 *
 *  @param[in] alarm - previous alarm state
 *  @param[in] direction - upper or lower threshold checking
 *  @param[in] value - raw value
 *  @param[in] threshold - threshold value
 *  @param[in] hyst - hysteresis value
 *  @return bool - new alarm state
 */
bool SensorIntf::checkThreshold(bool alarm, bool direction, double value,
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

void SensorIntf::updateThresholds()
{
    auto value = valueIntf->value();
    if (thresholdWarningIntf &&
        !std::isnan(thresholdWarningIntf->warningHigh()))
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
    }

    if (thresholdWarningIntf && !std::isnan(thresholdWarningIntf->warningLow()))
    {
        auto threshold = thresholdWarningIntf->warningLow();
        auto alarm = thresholdWarningIntf->warningAlarmLow();
        auto newAlarm =
            checkThreshold(alarm, false, value, threshold, hysteresis);
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

std::vector<uint8_t> SensorIntf::getReadingRequest()
{
    std::vector<uint8_t> request;
    request.resize(sizeof(pldm_msg_hdr) + PLDM_GET_SENSOR_READING_REQ_BYTES);
    auto requestMsg = reinterpret_cast<pldm_msg*>(request.data());
    auto rc = encode_get_sensor_reading_req(0, sensorId, false, requestMsg);
    if (rc)
    {
        std::cerr << "encode_get_sensor_reading_req failed, TID="
                  << unsigned(tid) << ", RC=" << unsigned(rc) << "\n";
        return {};
    }

    return request;
}

int SensorIntf::handleReadingResponse(const pldm_msg* responseMsg,
                                      size_t responseLen)
{
    if (!responseMsg)
    {
        return PLDM_ERROR;
    }

    uint8_t completionCode = PLDM_SUCCESS;
    uint8_t sensorDataSize = PLDM_SENSOR_DATA_SIZE_SINT32;
    uint8_t sensorOperationalState = 0;
    uint8_t sensorEventMessageEnable = 0;
    uint8_t presentState = 0;
    uint8_t previousState = 0;
    uint8_t eventState = 0;
    union_sensor_data_size presentReading;
    auto rc = decode_get_sensor_reading_resp(
        responseMsg, responseLen, &completionCode, &sensorDataSize,
        &sensorOperationalState, &sensorEventMessageEnable, &presentState,
        &previousState, &eventState,
        reinterpret_cast<uint8_t*>(&presentReading));
    if (rc)
    {
        std::cerr << "Failed to decode response of GetSensorReading, TID="
                  << unsigned(tid) << ", RC=" << unsigned(rc) << "\n";
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
        case PLDM_SENSOR_ENABLED:
            break;
        case PLDM_SENSOR_DISABLED:
            updateReading(true, false, 0);
            return completionCode;
        case PLDM_SENSOR_UNAVAILABLE:
        default:
            updateReading(false, false, 0);
            return completionCode;
    }

    double value;
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
