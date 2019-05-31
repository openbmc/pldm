#pragma once

#include "config.h"

#include "libpldmresponder/effecters.hpp"
#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>

#include <exception>
#include <map>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>
#include <variant>
#include <vector>

#include "libpldm/platform.h"
#include "libpldm/states.h"

namespace pldm
{

using Response = std::vector<uint8_t>;

namespace responder
{

/**
 *  @class DBusHandler
 *
 *  Expose API to to actually handle the D-Bus calls
 *
 *  This class contains the APIs to handle the D-Bus calls in platform area
 *  to cater the request from Host.
 *  A class is created to mock the apis in the test cases
 */
class DBusHandler
{
  public:
    /** @brief API to set a D-Bus property in platform area
     *
     *  @param[in] path - Object path for the D-Bus object
     *  @param[in] dbusProp - The D-Bus property
     *  @param[in] dbusInterface - The D-Bus interface
     *  @param[in] value - The value to be set
     *
     */
    int setDbusProperty(const std::string& objPath, const std::string& dbusProp,
                        const std::string& dbusInterface,
                        const std::variant<std::string>& value);
};

/** @brief Handler for GetPDR
 *
 *  @param[in] request - Request message payload
 *  @param[in] payloadLength - Request payload length
 *  @param[out] Response - Response message written here
 */
Response getPDR(const pldm_msg* request, size_t payloadLength);

/** @brief Handler for setStateEffecterStates
 *
 *  @param[in] request - Request message payload
 *  @param[in] payloadLength - Request payload length
 *  @param[out] Response - Response message written here
 */
Response setStateEffecterStates(const pldm_msg* request, size_t payloadLength);

/** @brief Function to actually set the effecter requested by host
 *  @param[in] dBusIntf - The interface object
 *  @param[in] effecterId - Effecter ID sent by the host to act on
 *  @param[in] stateField - The state field data for each of the states, equal
 *        to composite effecter count in number
 *  @param[out] - Success or failure in setting the states. Returns failure if
 *        atleast one state fails to be set
 */
template <class DBusInterface>
int setStateEffecterStatesHandler(
    DBusInterface* dBusIntf, effecter::Id effecterId,
    const std::vector<set_effecter_state_field>& stateField)
{
    using DBusProperty = std::string;
    using stateSetId = uint16_t;
    using stateSetNum = uint8_t;
    constexpr uint8_t firstRecordHndl = 1;
    std::map<stateSetId, std::map<stateSetNum, DBusProperty>> stateToDbusProp =
        {
            {PLDM_BOOT_PROGRESS_STATE,
             {{1, "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus."
                  "Standby"},
              {2, "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus."
                  "BootComplete"}}},
        };
    using namespace phosphor::logging;
    using namespace pldm::responder::pdr;
    using namespace pldm::responder::effecter::dbus_mapping;

    int rc = PLDM_SUCCESS;

    Repo& pdrRepo = get(PDR_JSONS_DIR);

    auto totalPdrEntries = pdrRepo.numEntries();
    size_t i = 1;
    auto foundPdr = false;
    pdr::Entry pdrEntry;
    auto recordHndl = firstRecordHndl;
    state_effecter_possible_states* states = nullptr;
    std::string objPath;
    std::string dbusProp;
    std::string dbusPropVal;
    std::string dbusInterface;
    uint8_t currState = 1;
    Paths paths;
    pldm_state_effecter_pdr* pdr;
    size_t compEffecterCnt = stateField.size();

    do
    {
        pdrEntry = pdrRepo.at(recordHndl);
        pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrEntry.data());
        recordHndl = pdr->hdr.record_handle;
        if (recordHndl == 0)
        {
            foundPdr = false;
            break;
        }

        if (pdr->effecter_id == effecterId)
        {
            foundPdr = true;
            states = reinterpret_cast<state_effecter_possible_states*>(
                pdr->possible_states);
            if (compEffecterCnt > pdr->composite_effecter_count)
            {
                foundPdr = false;
                break;
            }
            break;
        }
        i++;
    } while (i <= totalPdrEntries);

    std::map<stateSetId, std::function<void()>> effecterToDbusEntries = {
        {PLDM_BOOT_PROGRESS_STATE, [&]() {
             for (const auto& stateSet : stateToDbusProp)
             {
                 if (stateSet.first == PLDM_BOOT_PROGRESS_STATE)
                 {
                     auto search = stateSet.second.find(
                         stateField[currState - 1].effecter_state);
                     if (search == stateSet.second.end())
                     {
                         log<level::ERR>(
                             "Invalid state field passed or field not "
                             "found",
                             entry("FIELD=%d", states->states[0].byte));
                         rc = PLDM_ERROR;
                         break;
                     }
                     if (stateField[currState - 1].set_request ==
                         PLDM_NO_CHANGE)
                     {
                         break;
                     }
                     dbusProp = "OperatingSystemState";
                     std::variant<std::string> value{search->second};
                     dbusInterface =
                         "xyz.openbmc_project.State.OperatingSystem.Status";
                     rc = dBusIntf->setDbusProperty(objPath, dbusProp,
                                                    dbusInterface, value);
                     if (rc != PLDM_SUCCESS)
                     {
                         break;
                     }

                     break;
                 }
             }
         }}};

    if (foundPdr)
    {
        for (currState = 1; currState <= compEffecterCnt; currState++)
        {
            auto search = effecterToDbusEntries.find(states->state_set_id);
            if (search == effecterToDbusEntries.end())
            {
                log<level::ERR>(
                    "Did not find the state set ",
                    entry("STATE=%d", (uint16_t)states->state_set_id));
                rc = PLDM_ERROR;
                break;
            }
            auto paths = get(effecterId);
            objPath = paths[currState - 1];
            search->second();
            states = reinterpret_cast<state_effecter_possible_states*>(
                pdr->possible_states +
                currState * sizeof(state_effecter_possible_states));
        }
    }
    else
    {
        log<level::ERR>("PDR not found", entry("EFFECTER_ID=%d", effecterId));
        rc = PLDM_ERROR_INVALID_DATA;
    }

    return rc;
}

} // namespace responder
} // namespace pldm
