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
 *  @class dBusHandler
 *
 *  Expose API to to actually handle the dbus calls
 *
 *  This class contains the apis to handle the dbus calls in platform area
 *  to cater the request from Host.
 *  A class is created to mock the apis in the test cases
 */
class dBusHandler
{
  public:
    /** @brief API to set a dbus property in platform area
     *
     *  @param[in] path - Object path for the dbus object
     *  @param[in] dbusProp - The dbus property
     *  @param[in] dbusInterface - The dbus interface
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
    DBusInterface* dBusIntf, uint16_t effecterId,
    const std::vector<set_effecter_state_field>& stateField)
{
    std::map<uint16_t, std::map<uint8_t, std::string>> stateToDbusProp = {
        {PLDM_BOOT_PROGRESS_STATE,
         {{1,
           "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.Standby"},
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
    pdr::Entry e;
    auto recordHndl = 1;
    state_effecter_possible_states* states;
    std::string objPath;
    std::string dbusProp;
    std::string dbusPropVal;
    std::string dbusInterface;
    uint8_t pdrCompEffCnt = 0;
    uint8_t currState = 1;
    Paths paths;
    pldm_state_effecter_pdr* pdr;
    uint8_t compEffecterCnt = stateField.size();

    do
    {
        e = pdrRepo.at(recordHndl);
        pdr = reinterpret_cast<pldm_state_effecter_pdr*>(e.data());
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
            pdrCompEffCnt = pdr->composite_effecter_count;
            if (compEffecterCnt > pdrCompEffCnt)
            {
                foundPdr = false;
                break;
            }
            break;
        }
        i++;
    } while (i <= totalPdrEntries);

    std::map<uint16_t, std::function<void()>> effecterToDbusEntries = {
        {PLDM_BOOT_PROGRESS_STATE, [&]() {
             for (auto const& stateSet : stateToDbusProp)
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

            for (const auto& effecterToDbus : effecterToDbusEntries)
            {
                try
                {
                    if (effecterToDbus.first == states->state_set_id)
                    {
                        auto paths = get(effecterId);
                        objPath = paths[currState - 1];
                        effecterToDbus.second();
                    }
                }
                catch (const std::exception& e)
                {
                    log<level::ERR>("Failed executing dbus call",
                                    entry("STATE=%d", effecterToDbus.first),
                                    entry("PATH=%s", objPath.c_str()),
                                    entry("ERROR=%s", e.what()));
                    rc = PLDM_ERROR;
                    break;
                }
            }
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
