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

namespace platform
{

/** @brief Register handlers for commands from the platform spec
 */
void registerHandlers();

} // namespace platform

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
 *  @return - Success or failure in setting the states. Returns failure in terms
 *        of PLDM completion codes if atleast one state fails to be set
 */
template <class DBusInterface>
int setStateEffecterStatesHandler(
    DBusInterface* dBusIntf, effecter::Id effecterId,
    const std::vector<set_effecter_state_field>& stateField)
{
    using DBusProperty = std::string;
    using StateSetId = uint16_t;
    using StateSetNum = uint8_t;
    using PropertyMap =
        std::map<StateSetId, std::map<StateSetNum, DBusProperty>>;
    PropertyMap stateToDbusProp = {
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

    auto foundPdr = false;
    state_effecter_possible_states* states = nullptr;
    Paths paths{};
    pldm_state_effecter_pdr* pdr = nullptr;
    size_t compEffecterCnt = stateField.size();

    uint32_t recordHndl{};
    size_t i = 1;
    Repo& pdrRepo = get(PDR_JSONS_DIR);
    auto totalPdrEntries = pdrRepo.numEntries();
    pdr::Entry pdrEntry;
    do
    {
        pdrEntry = pdrRepo.at(recordHndl);
        pdr = reinterpret_cast<pldm_state_effecter_pdr*>(pdrEntry.data());
        recordHndl = pdr->hdr.record_handle;
        if (recordHndl == 0)
        {
            return PLDM_ERROR_INVALID_DATA;
        }
        if (pdr->effecter_id == effecterId)
        {
            foundPdr = true;
            states = reinterpret_cast<state_effecter_possible_states*>(
                pdr->possible_states);
            if (compEffecterCnt > pdr->composite_effecter_count)
            {
                return PLDM_ERROR_INVALID_DATA;
            }
            break;
        }
        i++;
        recordHndl = pdrRepo.getNextRecordHandle(recordHndl);
    } while (i <= totalPdrEntries);

    if (!foundPdr)
    {
        log<level::ERR>("PDR not found", entry("EFFECTER_ID=%d", effecterId));
        return PLDM_ERROR_INVALID_DATA;
    }

    std::map<StateSetId,
             std::function<int(
                 const PropertyMap& stateToDbusProp, const std::string& objPath,
                 const uint8_t currState,
                 const std::vector<set_effecter_state_field>& stateField,
                 DBusInterface* dBusIntf)>>
        effecterToDbusEntries = {
            {PLDM_BOOT_PROGRESS_STATE,
             [](const PropertyMap& stateToDbusProp, const std::string& objPath,
                const uint8_t currState,
                const std::vector<set_effecter_state_field>& stateField,
                DBusInterface* dBusIntf) {
                 int rc = PLDM_SUCCESS;
                 for (const auto& stateSet : stateToDbusProp)
                 {
                     if (stateSet.first == PLDM_BOOT_PROGRESS_STATE)
                     {
                         auto iter = stateSet.second.find(
                             stateField[currState - 1].effecter_state);
                         if (iter == stateSet.second.end())
                         {
                             log<level::ERR>(
                                 "Invalid state field passed or field not "
                                 "found",
                                 entry(
                                     "FIELD=%d",
                                     stateField[currState - 1].effecter_state));
                             rc = PLDM_ERROR;
                             break;
                         }
                         if (stateField[currState - 1].set_request ==
                             PLDM_NO_CHANGE)
                         {
                             break;
                         }
                         std::string dbusProp = "OperatingSystemState";
                         std::variant<std::string> value{iter->second};
                         std::string dbusInterface =
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
                 return rc;
             }}};

    std::string objPath;
    for (uint8_t currState = 1; currState <= compEffecterCnt; currState++)
    {
        auto iter = effecterToDbusEntries.find(states->state_set_id);
        if (iter == effecterToDbusEntries.end())
        {
            log<level::ERR>("Did not find the state set ",
                            entry("STATE=%d", (uint16_t)states->state_set_id));
            rc = PLDM_ERROR;
            break;
        }
        auto paths = get(effecterId);
        objPath = paths[currState - 1];
        rc = iter->second(stateToDbusProp, objPath, currState, stateField,
                          dBusIntf);
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
