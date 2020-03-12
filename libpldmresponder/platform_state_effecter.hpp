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
namespace platform_state_effecter
{
/** @brief Function to set the effecter requested by pldm requester
 *  @param[in] dBusIntf - The interface object
 *  @param[in] handler - The interface object
 *  @param[in] effecterId - Effecter ID sent by the requester to act on
 *  @param[in] stateField - The state field data for each of the states,
 * equal to composite effecter count in number
 *  @return - Success or failure in setting the states. Returns failure in
 * terms of PLDM completion codes if atleast one state fails to be set
 */
template <class DBusInterface, class Handler>
int setStateEffecterStatesHandler(
    const DBusInterface& dBusIntf, Handler& handler, uint16_t effecterId,
    const std::vector<set_effecter_state_field>& stateField)
{
    using namespace pldm::responder::pdr;
    using namespace pldm::utils;
    using StateSetNum = uint8_t;

    state_effecter_possible_states* states = nullptr;
    pldm_state_effecter_pdr* pdr = nullptr;
    uint8_t compEffecterCnt = stateField.size();

    pdr_utils::Repo repo =
        getRepoByType(handler.getRepo(), PLDM_STATE_EFFECTER_PDR);
    if (repo.empty())
    {
        std::cerr << "Failed to get record by PDR type\n";
        return PLDM_PLATFORM_INVALID_EFFECTER_ID;
    }

    pdr_utils::PdrEntry pdrEntry{};
    auto pdrRecord = repo.getFirstRecord(pdrEntry);
    while (pdrRecord)
    {
        pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrEntry.data);
        if (pdr->effecter_id != effecterId)
        {
            pdr = nullptr;
            pdrRecord = repo.getNextRecord(pdrRecord, pdrEntry);
            continue;
        }

        states = reinterpret_cast<state_effecter_possible_states*>(
            pdr->possible_states);
        if (compEffecterCnt > pdr->composite_effecter_count)
        {
            std::cerr << "The requester sent wrong composite effecter"
                      << " count for the effecter, EFFECTER_ID=" << effecterId
                      << "COMP_EFF_CNT=" << compEffecterCnt << "\n";
            return PLDM_ERROR_INVALID_DATA;
        }
        break;
    }

    if (!pdr)
    {
        return PLDM_PLATFORM_INVALID_EFFECTER_ID;
    }

    int rc = PLDM_SUCCESS;
    try
    {
        const auto& dbusObjs = handler.getDbusObjs(effecterId);
        const auto& dbusValMaps = handler.getDbusValMaps(effecterId);
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
                std::cerr << "Invalid state set value, EFFECTER_ID="
                          << effecterId
                          << " VALUE=" << stateField[currState].effecter_state
                          << " COMPOSITE_EFFECTER_ID=" << currState
                          << " DBUS_PATH=" << dbusObjs[currState].objectPath
                          << "\n";
                rc = PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE;
                break;
            }
            const DBusMapping& dbusMapping = dbusObjs[currState];
            const DbusIdToValMap& dbusValToMap = dbusValMaps[currState];
            if (stateField[currState].set_request == PLDM_REQUEST_SET)
            {
                try
                {
                    dBusIntf.setDbusProperty(
                        dbusMapping,
                        dbusValToMap.at(stateField[currState].effecter_state));
                }
                catch (const std::exception& e)
                {
                    std::cerr
                        << "Error setting property, ERROR=" << e.what()
                        << " PROPERTY=" << dbusMapping.propertyName
                        << " INTERFACE=" << dbusMapping.interface << " PATH="
                        << dbusMapping.objectPath << "\n";
                    return PLDM_ERROR;
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
    }
    catch (const std::out_of_range& e)
    {
        std::cerr << "the effecterId does not exist. effecter id: "
                  << effecterId << e.what() << '\n';
    }

    return rc;
}

} // namespace platform_state_effecter
} // namespace responder
} // namespace pldm
