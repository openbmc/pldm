#pragma once

#include "config.h"

#include "handler.hpp"
#include "libpldmresponder/pdr.hpp"
#include "pdr_utils.hpp"
#include "utils.hpp"

#include <cstdint>
#include <map>

#include "libpldm/platform.h"
#include "libpldm/states.h"

using namespace pldm::responder::pdr;

namespace pldm
{
namespace responder
{
namespace platform_state_sensor
{

/** @brief Function to get the present state
 *
 *  @tparam[in] DBusInterface - DBus interface type
 *  @param[in] dBusIntf - The interface object of DBusInterface
 *  @param[in] path - The d-bus object path
 *
 *  @return - Enumeration of PresentState
 */
template <class DBusInterface>
uint8_t getPresentState(const DBusInterface& dBusIntf, std::string path)
{
    constexpr auto normalInterface = "xyz.openbmc_project.Sensor.Value";
    constexpr auto normalValue = "Value";

    constexpr auto warningInterface =
        "xyz.openbmc_project.Sensor.Threshold.Warning";
    constexpr auto warningLowValue = "WarningAlarmLow";
    constexpr auto warningHighValue = "WarningAlarmHigh";

    constexpr auto criticalInterface =
        "xyz.openbmc_project.Sensor.Threshold.Critical";
    constexpr auto criticalLowValue = "CriticalAlarmLow";
    constexpr auto criticalHighValue = "CriticalAlarmHigh";

    try
    {
        // critical
        auto propertyCriticalLowValue = dBusIntf.getDbusPropertyVariant(
            path.c_str(), criticalLowValue, criticalInterface);
        auto criticalLow = std::get<bool>(propertyCriticalLowValue);
        if (criticalLow)
        {
            return LOWERCRITICAL;
        }

        auto propertyCriticalHighValue = dBusIntf.getDbusPropertyVariant(
            path.c_str(), criticalHighValue, criticalInterface);
        auto criticalHigh = std::get<bool>(propertyCriticalHighValue);
        if (criticalHigh)
        {
            return UPPERCRITICAL;
        }

        // warning
        auto propertyWarningLowValue = dBusIntf.getDbusPropertyVariant(
            path.c_str(), warningLowValue, warningInterface);
        auto warningLow = std::get<bool>(propertyWarningLowValue);
        if (warningLow)
        {
            return LOWERWARNING;
        }

        auto propertyWarningHighValue = dBusIntf.getDbusPropertyVariant(
            path.c_str(), warningHighValue, warningInterface);
        auto warningHigh = std::get<bool>(propertyWarningHighValue);
        if (warningHigh)
        {
            return UPPERWARNING;
        }

        // normal
        auto propertyNormalValue = dBusIntf.getDbusPropertyVariant(
            path.c_str(), normalValue, normalInterface);
        auto normal = std::get<uint64_t>(propertyNormalValue);
        if (normal > 0)
        {
            return NORMAL;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    return UNKNOWN;
}

/** @brief Function to get the Bitfield count to 1
 *
 *  @param[in] bit - Bitfield
 *
 *  @return - uint8_t return the number of 1
 */
uint8_t getBitfieldCount(const bitfield8_t bit)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < 8; ++i)
    {
        if (bit.byte & (1 << i))
        {
            ++count;
        }
    }

    return count;
}

/** @brief Function to get the state sensor readings requested by pldm requester
 *
 *  @tparam[in] DBusInterface - DBus interface type
 *  @tparam[in] Handler - pldm::responder::platform::Handler
 *  @param[in] dBusIntf - The interface object of DBusInterface
 *  @param[in] handler - The interface object of
 *             pldm::responder::platform::Handler
 *  @param[in] sensorId - Sensor ID sent by the requester to act on
 *  @param[in] sensorRearm - Each bit location in this field corresponds to a
 *              particular sensor within the state sensor
 *  @param[out] compSensorCnt - composite sensor count
 *  @param[out] stateField - The state field data for each of the states,
 *              equal to composite sensor count in number
 *  @return - Success or failure in setting the states. Returns failure in
 * terms of PLDM completion codes if atleast one state fails to be set
 */
template <class DBusInterface, class Handler>
int getStateSensorReadingsHandler(
    const DBusInterface& dBusIntf, Handler& handler, uint16_t sensorId,
    bitfield8_t sensorRearm, uint8_t& compSensorCnt,
    std::vector<get_sensor_state_field>& stateField)
{
    using namespace pldm::responder::pdr;
    using namespace pldm::utils;

    pldm_state_sensor_pdr* pdr = nullptr;

    std::unique_ptr<pldm_pdr, decltype(&pldm_pdr_destroy)> stateSensorPdrRepo(
        pldm_pdr_init(), pldm_pdr_destroy);
    Repo stateSensorPDRs(stateSensorPdrRepo.get());
    getRepoByType(handler.getRepo(), stateSensorPDRs, PLDM_STATE_SENSOR_PDR);
    if (stateSensorPDRs.empty())
    {
        std::cerr << "Failed to get record by PDR type\n";
        return PLDM_PLATFORM_INVALID_SENSOR_ID;
    }

    PdrEntry pdrEntry{};
    auto pdrRecord = stateSensorPDRs.getFirstRecord(pdrEntry);
    while (pdrRecord)
    {
        pdr = reinterpret_cast<pldm_state_sensor_pdr*>(pdrEntry.data);
        if (pdr->sensor_id != sensorId)
        {
            pdr = nullptr;
            pdrRecord = stateSensorPDRs.getNextRecord(pdrRecord, pdrEntry);
            continue;
        }

        compSensorCnt = pdr->composite_sensor_count;
        if (getBitfieldCount(sensorRearm) > compSensorCnt)
        {
            std::cerr << "The requester sent wrong sensorRearm"
                      << " count for the sensor, SENSOR_ID=" << sensorId
                      << "SENSOR_REARM=" << sensorRearm.byte << "\n";
            return PLDM_PLATFORM_INVALID_SENSOR_REARM;
        }
        break;
    }

    if (!pdr)
    {
        return PLDM_PLATFORM_INVALID_SENSOR_ID;
    }

    int rc = PLDM_SUCCESS;
    try
    {
        auto& dbusPropertyMaps = handler.getDbusObjPropertyMaps(sensorId);

        stateField.clear();
        for (size_t i = 0; i < compSensorCnt; i++)
        {
            auto& statestoDbusProperty = dbusPropertyMaps[i];

            auto iter = statestoDbusProperty.begin();
            if (iter == statestoDbusProperty.end())
            {
                stateField.push_back({DISABLED, UNKNOWN, UNKNOWN, UNKNOWN});
            }
            else
            {
                auto dbusObj = iter->second;
                uint8_t presentState = getPresentState<DBusInterface>(
                    dBusIntf, dbusObj.objectPath);
                stateField.push_back({ENABLED, presentState, UNKNOWN, UNKNOWN});
            }
        }
    }
    catch (const std::out_of_range& e)
    {
        std::cerr << "the sensorId does not exist. sensor id: " << sensorId
                  << e.what() << '\n';
    }

    return rc;
}

} // namespace platform_state_sensor
} // namespace responder
} // namespace pldm
