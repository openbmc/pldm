#pragma once

#include "config.h"

#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/utils.hpp"

#include <stdint.h>

#include <map>

#include "libpldm/platform.h"
#include "libpldm/states.h"

namespace pldm
{

using Response = std::vector<uint8_t>;

namespace responder
{

namespace platform
{

/** @brief Register handlers for commands from the platform spec
 */
void registerHandlers();

} // namespace platform

/** @brief Handler for GetPDR
 *
 *  @param[in] request - Request message
 *  @param[in] payloadLength - Request payload length
 *  @return Response - PLDM Response message
 */
Response getPDR(const pldm_msg* request, size_t payloadLength);

/** @brief Handler for setStateEffecterStates
 *
 *  @param[in] request - Request message
 *  @param[in] payloadLength - Request payload length
 *  @return Response - PLDM Response message
 */
Response setStateEffecterStates(const pldm_msg* request, size_t payloadLength);

/** @brief Function to set the effecter requested by pldm requester
 *  @param[in] dBusIntf - The interface object
 *  @param[in] effecterId - Effecter ID sent by the requester to act on
 *  @param[in] stateField - The state field data for each of the states, equal
 *        to composite effecter count in number
 *  @return - Success or failure in setting the states. Returns failure in terms
 *        of PLDM completion codes if atleast one state fails to be set
 */
template <class DBusInterface>
int setStateEffecterStatesHandler(
    const DBusInterface* dBusIntf, effecter::Id effecterId,
    const std::vector<set_effecter_state_field>& stateField)
{
    using DBusProperty = std::string;
    using StateSetId = uint16_t;
    using StateSetNum = uint8_t;
    using PropertyMap =
        std::map<StateSetId, std::map<StateSetNum, DBusProperty>>;
    static const PropertyMap stateNumToDbusProp = {
        {PLDM_BOOT_PROGRESS_STATE,
         {{PLDM_BOOT_NOT_ACTIVE,
           "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus."
           "Standby"},
          {PLDM_BOOT_COMPLETED,
           "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus."
           "BootComplete"}}},
        {PLDM_SYSTEM_POWER_STATE,
         {{9, "xyz.openbmc_project.State.Chassis.Transition.Off"}}}};
    using namespace phosphor::logging;
    using namespace pldm::responder::pdr;
    using namespace pldm::responder::effecter::dbus_mapping;

    state_effecter_possible_states* states = nullptr;
    pldm_state_effecter_pdr* pdr = nullptr;
    uint8_t compEffecterCnt = stateField.size();
    uint32_t recordHndl{};
    Repo& pdrRepo = get(PDR_JSONS_DIR);
    pdr::Entry pdrEntry;

    while (!pdr)
    {
        pdrEntry = pdrRepo.at(recordHndl);
        pldm_pdr_hdr* header = reinterpret_cast<pldm_pdr_hdr*>(pdrEntry.data());
        if (header->type != PLDM_STATE_EFFECTER_PDR)
        {
            recordHndl = pdrRepo.getNextRecordHandle(recordHndl);
            if (recordHndl)
            {
                continue;
            }
            return PLDM_PLATFORM_INVALID_EFFECTER_ID;
        }
        pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrEntry.data());
        recordHndl = pdr->hdr.record_handle;
        if (pdr->effecter_id == effecterId)
        {
            states = reinterpret_cast<state_effecter_possible_states*>(
                pdr->possible_states);
            if (compEffecterCnt > pdr->composite_effecter_count)
            {
                log<level::DEBUG>("The requester sent wrong composite effecter "
                                  "count for the effecter",
                                  entry("EFFECTER_ID=%d", effecterId),
                                  entry("COMP_EFF_CNT=%d", compEffecterCnt));
                return PLDM_ERROR_INVALID_DATA;
            }
            break;
        }
        recordHndl = pdrRepo.getNextRecordHandle(recordHndl);
        if (!recordHndl)
        {
            return PLDM_PLATFORM_INVALID_EFFECTER_ID;
        }
        pdr = nullptr;
    }

    std::map<StateSetId,
             std::function<int(
                 const PropertyMap& stateNumToDbusProp,
                 const std::string& objPath, const uint8_t currState,
                 const std::vector<set_effecter_state_field>& stateField,
                 const DBusInterface* dBusIntf)>>
        effecterToDbusEntries = {
            {PLDM_BOOT_PROGRESS_STATE,
             [](const PropertyMap& stateNumToDbusProp,
                const std::string& objPath, const uint8_t currState,
                const std::vector<set_effecter_state_field>& stateField,
                const DBusInterface* dBusIntf) {
                 auto stateSet =
                     stateNumToDbusProp.find(PLDM_BOOT_PROGRESS_STATE);
                 auto iter = stateSet->second.find(
                     stateField[currState - 1].effecter_state);
                 if (iter == stateSet->second.end())
                 {
                     log<level::ERR>(
                         "Invalid state field passed or field not "
                         "found for PLDM_BOOT_PROGRESS_STATE",
                         entry("FIELD=%d",
                               stateField[currState - 1].effecter_state),
                         entry("OBJECT_PATH=%s", objPath.c_str()));
                     return (int)PLDM_ERROR_INVALID_DATA;
                 }
                 std::string dbusProp = "OperatingSystemState";
                 std::variant<std::string> value{iter->second};
                 std::string dbusInterface =
                     "xyz.openbmc_project.State.OperatingSystem.Status";
                 int rc = PLDM_SUCCESS;
                 try
                 {
                     rc = dBusIntf->setDbusProperty(objPath.c_str(),
                                                    dbusProp.c_str(),
                                                    dbusInterface, value);
                 }
                 catch (const std::exception& e)
                 {
                     log<level::ERR>(
                         "Error setting property",
                         entry("PROPERTY=%s", dbusProp.c_str()),
                         entry("INTERFACE=%s", dbusInterface.c_str()),
                         entry("PATH=%s", objPath.c_str()));
                     return (int)PLDM_ERROR;
                 }
                 return rc;
             }},
            {PLDM_SYSTEM_POWER_STATE,
             [](const PropertyMap& stateNumToDbusProp,
                const std::string& objPath, const uint8_t currState,
                const std::vector<set_effecter_state_field>& stateField,
                const DBusInterface* dBusIntf) {
                 auto stateSet =
                     stateNumToDbusProp.find(PLDM_SYSTEM_POWER_STATE);
                 auto iter = stateSet->second.find(
                     stateField[currState - 1].effecter_state);
                 if (iter == stateSet->second.end())
                 {
                     log<level::ERR>(
                         "Invalid state field passed or field not "
                         "found for PLDM_SYSTEM_POWER_STATE",
                         entry("FIELD=%d",
                               stateField[currState - 1].effecter_state),
                         entry("OBJECT_PATH=%s", objPath.c_str()));
                     return (int)PLDM_ERROR_INVALID_DATA;
                 }
                 std::string dbusProp = "RequestedPowerTransition";
                 std::variant<std::string> value{iter->second};
                 std::string dbusInterface =
                     "xyz.openbmc_project.State.Chassis";
                 int rc = PLDM_SUCCESS;
                 try
                 {
                     rc = dBusIntf->setDbusProperty(objPath.c_str(),
                                                    dbusProp.c_str(),
                                                    dbusInterface, value);
                 }
                 catch (const std::exception& e)
                 {
                     log<level::ERR>(
                         "Error setting property",
                         entry("PROPERTY=%s", dbusProp.c_str()),
                         entry("INTERFACE=%s", dbusInterface.c_str()),
                         entry("PATH=%s", objPath.c_str()));
                     return (int)PLDM_ERROR;
                 }
                 return rc;
             }}};

    int rc = PLDM_SUCCESS;
    auto paths = get(effecterId);
    for (uint8_t currState = 1; currState <= compEffecterCnt; currState++)
    {
        auto iter = effecterToDbusEntries.find(states->state_set_id);
        if (iter == effecterToDbusEntries.end())
        {
            log<level::ERR>(
                "Did not find the state set for the state effecter pdr  ",
                entry("STATE=%d", (uint16_t)states->state_set_id),
                entry("EFFECTER_ID=%d", effecterId));
            rc = PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE;
            break;
        }
        if (stateField[currState - 1].set_request == PLDM_NO_CHANGE)
        {
            states = reinterpret_cast<state_effecter_possible_states*>(
                pdr->possible_states +
                currState * sizeof(state_effecter_possible_states));

            continue;
        }

        rc = iter->second(stateNumToDbusProp, paths[currState - 1], currState,
                          stateField, dBusIntf);
        if (rc != PLDM_SUCCESS)
        {
            break;
        }
        states = reinterpret_cast<state_effecter_possible_states*>(
            pdr->possible_states +
            currState * sizeof(state_effecter_possible_states));
    }
    return rc;
}

} // namespace responder
} // namespace pldm
