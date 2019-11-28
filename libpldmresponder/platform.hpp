#pragma once

#include "config.h"

#include "handler.hpp"
#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/utils.hpp"

#include <stdint.h>

#include <map>

#include "libpldm/platform.h"
#include "libpldm/states.h"

namespace pldm
{
namespace responder
{
namespace platform
{

class Handler : public CmdHandler
{
  public:
    Handler()
    {
        handlers.emplace(PLDM_GET_PDR,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->getPDR(request, payloadLength);
                         });
        handlers.emplace(PLDM_SET_STATE_EFFECTER_STATES,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->setStateEffecterStates(request,
                                                                 payloadLength);
                         });
    }

    /** @brief Handler for GetPDR
     *
     *  @param[in] request - Request message payload
     *  @param[in] payloadLength - Request payload length
     *  @param[out] Response - Response message written here
     */
    Response getPDR(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for setStateEffecterStates
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @return Response - PLDM Response message
     */
    Response setStateEffecterStates(const pldm_msg* request,
                                    size_t payloadLength);

    /** @brief Function to set the effecter requested by pldm requester
     *  @param[in] dBusIntf - The interface object
     *  @param[in] effecterId - Effecter ID sent by the requester to act on
     *  @param[in] stateField - The state field data for each of the states,
     * equal to composite effecter count in number
     *  @return - Success or failure in setting the states. Returns failure in
     * terms of PLDM completion codes if atleast one state fails to be set
     */
    template <class DBusInterface>
    int setStateEffecterStatesHandler(
        const DBusInterface& dBusIntf, effecter::Id effecterId,
        const std::vector<set_effecter_state_field>& stateField)
    {
        using namespace std::string_literals;
        using DBusProperty = std::variant<std::string, bool>;
        using StateSetId = uint16_t;
        using StateSetNum = uint8_t;
        using PropertyMap =
            std::map<StateSetId, std::map<StateSetNum, DBusProperty>>;
        static const PropertyMap stateNumToDbusProp = {
            {PLDM_BOOT_PROGRESS_STATE,
             {{PLDM_BOOT_NOT_ACTIVE,
               "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus."
               "Standby"s},
              {PLDM_BOOT_COMPLETED,
               "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus."
               "BootComplete"s}}},
            {PLDM_SYSTEM_POWER_STATE,
             {{PLDM_OFF_SOFT_GRACEFUL,
               "xyz.openbmc_project.State.Chassis.Transition.Off"s}}}};
        using namespace phosphor::logging;
        using namespace pldm::responder::pdr;
        using namespace pldm::responder::effecter::dbus_mapping;

        state_effecter_possible_states* states = nullptr;
        pldm_state_effecter_pdr* pdr = nullptr;
        uint8_t compEffecterCnt = stateField.size();
        uint32_t recordHndl{};
        Repo& pdrRepo = get(PDR_JSONS_DIR);
        pdr::Entry pdrEntry{};

        while (!pdr)
        {
            pdrEntry = pdrRepo.at(recordHndl);
            pldm_pdr_hdr* header =
                reinterpret_cast<pldm_pdr_hdr*>(pdrEntry.data());
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
                    log<level::ERR>(
                        "The requester sent wrong composite effecter "
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

        std::map<StateSetId, std::function<int(const std::string& objPath,
                                               const uint8_t currState)>>
            effecterToDbusEntries = {
                {PLDM_BOOT_PROGRESS_STATE,
                 [&](const std::string& objPath, const uint8_t currState) {
                     auto stateSet =
                         stateNumToDbusProp.find(PLDM_BOOT_PROGRESS_STATE);
                     if (stateSet == stateNumToDbusProp.end())
                     {
                         log<level::ERR>("Couldn't find D-Bus mapping for "
                                         "PLDM_BOOT_PROGRESS_STATE",
                                         entry("EFFECTER_ID=%d", effecterId));
                         return PLDM_ERROR;
                     }
                     auto iter = stateSet->second.find(
                         stateField[currState].effecter_state);
                     if (iter == stateSet->second.end())
                     {
                         log<level::ERR>(
                             "Invalid state field passed or field not "
                             "found for PLDM_BOOT_PROGRESS_STATE",
                             entry("EFFECTER_ID=%d", effecterId),
                             entry("FIELD=%d",
                                   stateField[currState].effecter_state),
                             entry("OBJECT_PATH=%s", objPath.c_str()));
                         return PLDM_ERROR_INVALID_DATA;
                     }
                     auto dbusProp = "OperatingSystemState";
                     std::variant<std::string> value{
                         std::get<std::string>(iter->second)};
                     auto dbusInterface =
                         "xyz.openbmc_project.State.OperatingSystem.Status";
                     try
                     {
                         dBusIntf.setDbusProperty(objPath.c_str(), dbusProp,
                                                  dbusInterface, value);
                     }
                     catch (const std::exception& e)
                     {
                         log<level::ERR>("Error setting property",
                                         entry("ERROR=%s", e.what()),
                                         entry("PROPERTY=%s", dbusProp),
                                         entry("INTERFACE=%s", dbusInterface),
                                         entry("PATH=%s", objPath.c_str()));
                         return PLDM_ERROR;
                     }
                     return PLDM_SUCCESS;
                 }},
                {PLDM_SYSTEM_POWER_STATE,
                 [&](const std::string& objPath, const uint8_t currState) {
                     auto stateSet =
                         stateNumToDbusProp.find(PLDM_SYSTEM_POWER_STATE);
                     if (stateSet == stateNumToDbusProp.end())
                     {
                         log<level::ERR>("Couldn't find D-Bus mapping for "
                                         "PLDM_SYSTEM_POWER_STATE",
                                         entry("EFFECTER_ID=%d", effecterId));
                         return PLDM_ERROR;
                     }
                     auto iter = stateSet->second.find(
                         stateField[currState].effecter_state);
                     if (iter == stateSet->second.end())
                     {
                         log<level::ERR>(
                             "Invalid state field passed or field not "
                             "found for PLDM_SYSTEM_POWER_STATE",
                             entry("EFFECTER_ID=%d", effecterId),
                             entry("FIELD=%d",
                                   stateField[currState].effecter_state),
                             entry("OBJECT_PATH=%s", objPath.c_str()));
                         return PLDM_ERROR_INVALID_DATA;
                     }
                     auto dbusProp = "RequestedPowerTransition";
                     std::variant<std::string> value{
                         std::get<std::string>(iter->second)};
                     auto dbusInterface = "xyz.openbmc_project.State.Chassis";
                     try
                     {
                         dBusIntf.setDbusProperty(objPath.c_str(), dbusProp,
                                                  dbusInterface, value);
                     }
                     catch (const std::exception& e)
                     {
                         log<level::ERR>("Error setting property",
                                         entry("ERROR=%s", e.what()),
                                         entry("PROPERTY=%s", dbusProp),
                                         entry("INTERFACE=%s", dbusInterface),
                                         entry("PATH=%s", objPath.c_str()));
                         return PLDM_ERROR;
                     }
                     return PLDM_SUCCESS;
                 }}};

        int rc = PLDM_SUCCESS;
        auto paths = get(effecterId);
        for (uint8_t currState = 0; currState < compEffecterCnt; ++currState)
        {
            std::vector<StateSetNum> allowed{};
            // computation is based on table 79 from DSP0248 v1.1.1
            uint8_t bitfieldIndex = stateField[currState].effecter_state / 8;
            uint8_t bit =
                stateField[currState].effecter_state - (8 * bitfieldIndex);
            if (states->possible_states_size < bitfieldIndex ||
                !(states->states[bitfieldIndex].byte & (1 << bit)))
            {
                log<level::ERR>(
                    "Invalid state set value",
                    entry("EFFECTER_ID=%d", effecterId),
                    entry("VALUE=%d", stateField[currState].effecter_state),
                    entry("COMPOSITE_EFFECTER_ID=%d", currState),
                    entry("DBUS_PATH=%c", paths[currState].c_str()));
                rc = PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE;
                break;
            }
            auto iter = effecterToDbusEntries.find(states->state_set_id);
            if (iter == effecterToDbusEntries.end())
            {
                uint16_t setId = states->state_set_id;
                log<level::ERR>(
                    "Did not find the state set for the state effecter pdr  ",
                    entry("STATE=%d", setId),
                    entry("EFFECTER_ID=%d", effecterId));
                rc = PLDM_PLATFORM_INVALID_STATE_VALUE;
                break;
            }
            if (stateField[currState].set_request == PLDM_REQUEST_SET)
            {
                rc = iter->second(paths[currState], currState);
                if (rc != PLDM_SUCCESS)
                {
                    break;
                }
            }
            uint8_t* nextState =
                reinterpret_cast<uint8_t*>(states) +
                sizeof(state_effecter_possible_states) -
                sizeof(states->states) +
                (states->possible_states_size * sizeof(states->states));
            states =
                reinterpret_cast<state_effecter_possible_states*>(nextState);
        }
        return rc;
    }
};

} // namespace platform
} // namespace responder
} // namespace pldm
