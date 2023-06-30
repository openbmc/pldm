#pragma once

#include "common/utils.hpp"
#include "libpldmresponder/pdr.hpp"
#include "pdr_utils.hpp"
#include "pldmd/handler.hpp"

#include <libpldm/platform.h>
#include <libpldm/states.h>

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <map>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{
namespace platform_state_sensor
{
/** @brief Function to get the sensor state
 *
 *  @tparam[in] DBusInterface - DBus interface type
 *  @param[in] dBusIntf - The interface object of DBusInterface
 *  @param[in] stateToDbusValue - Map of DBus property State to attribute value
 *  @param[in] dbusMapping - The d-bus object
 *
 *  @return - Enumeration of SensorState
 */
template <class DBusInterface>
uint8_t getStateSensorEventState(
    const DBusInterface& dBusIntf,
    const std::map<pldm::responder::pdr_utils::State,
                   pldm::utils::PropertyValue>& stateToDbusValue,
    const pldm::utils::DBusMapping& dbusMapping)
{
    try
    {
        auto propertyValue = dBusIntf.getDbusPropertyVariant(
            dbusMapping.objectPath.c_str(), dbusMapping.propertyName.c_str(),
            dbusMapping.interface.c_str());

        for (const auto& stateValue : stateToDbusValue)
        {
            if (stateValue.second == propertyValue)
            {
                return stateValue.first;
            }
        }
    }
    catch (const std::exception& e)
    {
        error(
            "Get StateSensor EventState from dbus Error, interface : {DBUS_OBJ_PATH}, exception : {ERR_EXCEP}",
            "DBUS_OBJ_PATH", dbusMapping.objectPath.c_str(), "ERR_EXCEP",
            e.what());
    }

    return PLDM_SENSOR_UNKNOWN;
}

/** @brief Function to get the state sensor readings requested by pldm requester
 *
 *  @tparam[in] DBusInterface - DBus interface type
 *  @tparam[in] Handler - pldm::responder::platform::Handler
 *  @param[in] dBusIntf - The interface object of DBusInterface
 *  @param[in] handler - The interface object of
 *             pldm::responder::platform::Handler
 *  @param[in] sensorId - Sensor ID sent by the requester to act on
 *  @param[in] sensorRearmCnt - Each bit location in this field corresponds to a
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
    uint8_t sensorRearmCnt, uint8_t& compSensorCnt,
    std::vector<get_sensor_state_field>& stateField)
{
    using namespace pldm::responder::pdr;
    using namespace pldm::utils;

    pldm_state_sensor_pdr* pdr = nullptr;

    std::unique_ptr<pldm_pdr, decltype(&pldm_pdr_destroy)> stateSensorPdrRepo(
        pldm_pdr_init(), pldm_pdr_destroy);
    if (!stateSensorPdrRepo)
    {
        throw std::runtime_error(
            "Failed to instantiate state sensor PDR repository");
    }
    pldm::responder::pdr_utils::Repo stateSensorPDRs(stateSensorPdrRepo.get());
    getRepoByType(handler.getRepo(), stateSensorPDRs, PLDM_STATE_SENSOR_PDR);
    if (stateSensorPDRs.empty())
    {
        error("Failed to get record by PDR type");
        return PLDM_PLATFORM_INVALID_SENSOR_ID;
    }

    pldm::responder::pdr_utils::PdrEntry pdrEntry{};
    auto pdrRecord = stateSensorPDRs.getFirstRecord(pdrEntry);
    while (pdrRecord)
    {
        pdr = reinterpret_cast<pldm_state_sensor_pdr*>(pdrEntry.data);
        assert(pdr != NULL);
        if (pdr->sensor_id != sensorId)
        {
            pdr = nullptr;
            pdrRecord = stateSensorPDRs.getNextRecord(pdrRecord, pdrEntry);
            continue;
        }

        compSensorCnt = pdr->composite_sensor_count;
        if (sensorRearmCnt > compSensorCnt)
        {
            error(
                "The requester sent wrong sensorRearm count for the sensor, SENSOR_ID={SENSOR_ID} SENSOR_REARM_COUNT={SENSOR_REARM_CNT}",
                "SENSOR_ID", sensorId, "SENSOR_REARM_CNT", sensorRearmCnt);
            return PLDM_PLATFORM_REARM_UNAVAILABLE_IN_PRESENT_STATE;
        }

        if (sensorRearmCnt == 0)
        {
            sensorRearmCnt = compSensorCnt;
            stateField.resize(sensorRearmCnt);
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
        const auto& [dbusMappings, dbusValMaps] = handler.getDbusObjMaps(
            sensorId, pldm::responder::pdr_utils::TypeId::PLDM_SENSOR_ID);

        stateField.clear();
        for (size_t i = 0; i < sensorRearmCnt; i++)
        {
            auto& dbusMapping = dbusMappings[i];

            uint8_t sensorEvent = getStateSensorEventState<DBusInterface>(
                dBusIntf, dbusValMaps[i], dbusMapping);

            uint8_t opState = PLDM_SENSOR_ENABLED;
            if (sensorEvent == PLDM_SENSOR_UNKNOWN)
            {
                opState = PLDM_SENSOR_UNAVAILABLE;
            }

            stateField.push_back({opState, PLDM_SENSOR_NORMAL,
                                  PLDM_SENSOR_UNKNOWN, sensorEvent});
        }
    }
    catch (const std::out_of_range& e)
    {
        error("the sensorId does not exist. sensor id: {SENSOR_ID} {ERR_EXCEP}",
              "SENSOR_ID", sensorId, "ERR_EXCEP", e.what());
        rc = PLDM_ERROR;
    }

    return rc;
}

} // namespace platform_state_sensor
} // namespace responder
} // namespace pldm
