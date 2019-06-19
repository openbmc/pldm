#include "config.h"

#include "platform.hpp"

#include "effecters.hpp"
#include "libpldmresponder/utils.hpp"
#include "pdr.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <exception>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <variant>

namespace pldm
{

constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";

namespace responder
{

std::map<uint16_t, std::map<uint8_t, std::string>> stateToDbusProp = {
    {PLDM_BOOT_PROGRESS_STATE,
     {{1, "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.Standby"},
      {2, "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus."
          "BootComplete"}}},
    {PLDM_SYSTEM_POWER_STATE,
     {{9, "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus."
          "SoftPowerOff"}}},
};

using namespace phosphor::logging;
using namespace pldm::responder::pdr;
using namespace pldm::responder::effecter::dbus_mapping;

Response getPDR(const pldm_msg* request, size_t payloadLength)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_MIN_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_GET_PDR_REQ_BYTES)
    {
        encode_get_pdr_resp(0, PLDM_ERROR_INVALID_LENGTH, 0, 0, 0, 0, nullptr,
                            0, responsePtr);
        return response;
    }

    uint32_t recordHandle{};
    uint32_t dataTransferHandle{};
    uint8_t transferOpFlag{};
    uint16_t reqSizeBytes{};
    uint16_t recordChangeNum{};
    decode_get_pdr_req(request->payload, payloadLength, &recordHandle,
                       &dataTransferHandle, &transferOpFlag, &reqSizeBytes,
                       &recordChangeNum);

    uint32_t nextRecordHandle{};
    uint16_t respSizeBytes{};
    uint8_t* recordData = nullptr;
    try
    {
        pdr::Repo& pdrRepo = pdr::get(PDR_JSONS_DIR);
        nextRecordHandle = pdrRepo.getNextRecordHandle(recordHandle);
        pdr::Entry e;
        if (reqSizeBytes)
        {
            e = pdrRepo.at(recordHandle);
            respSizeBytes = e.size();
            if (respSizeBytes > reqSizeBytes)
            {
                respSizeBytes = reqSizeBytes;
            }
            recordData = e.data();
        }
        response.resize(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_MIN_RESP_BYTES +
                            respSizeBytes,
                        0);
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        encode_get_pdr_resp(0, PLDM_SUCCESS, nextRecordHandle, 0, PLDM_START,
                            respSizeBytes, recordData, 0, responsePtr);
    }
    catch (const std::out_of_range& e)
    {
        encode_get_pdr_resp(0, PLDM_PLATFORM_INVALID_RECORD_HANDLE,
                            nextRecordHandle, 0, PLDM_START, respSizeBytes,
                            recordData, 0, responsePtr);
        return response;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error accessing PDR", entry("HANDLE=%d", recordHandle),
                        entry("ERROR=%s", e.what()));
        encode_get_pdr_resp(0, PLDM_ERROR, nextRecordHandle, 0, PLDM_START,
                            respSizeBytes, recordData, 0, responsePtr);
        return response;
    }
    return response;
}

Response setStateEffecterStates(const pldm_msg* request, size_t payloadLength)
{
    Response response(
        sizeof(pldm_msg_hdr) + PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint16_t effecterId;
    uint8_t compEffecterCnt;
    std::array<set_effecter_state_field, 8> stateField{};

    if ((payloadLength > PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES) ||
        (payloadLength < sizeof(effecterId) + sizeof(compEffecterCnt) +
                             sizeof(set_effecter_state_field)))
    {
        encode_set_state_effecter_states_resp(0, PLDM_ERROR_INVALID_LENGTH,
                                              responsePtr);
        return response;
    }

    int rc = decode_set_state_effecter_states_req(
        request->payload, payloadLength, &effecterId, &compEffecterCnt,
        stateField.data());
    if (rc == PLDM_SUCCESS)
    {
        rc = setBmcEffecter(effecterId, compEffecterCnt, stateField.data());
    }

    encode_set_state_effecter_states_resp(0, rc, responsePtr);
    return response;
}

int setBmcEffecter(uint16_t& effecterId, uint8_t& compEffecterCnt,
                   set_effecter_state_field* stateField)
{
    int rc = PLDM_SUCCESS;

    Repo& pdrRepo = get(PDR_JSONS_DIR);

    auto totalPdrEntries = pdrRepo.numEntries();
    size_t i = 1;
    auto foundPdr = false;
    pdr::Entry e;
    auto recordHndl = 1;
    state_effecter_or_sensor_possible_states* states;
    std::string objPath;
    std::string dbusProp;
    std::string dbusPropVal;
    std::string dbusInterface;
    uint8_t pdrCompEffCnt = 0;
    uint8_t currState = 1;
    Paths paths;
    pldm_state_effecter_pdr* pdr;

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

        if (pdr->hdr.type == PLDM_STATE_EFFECTER_PDR &&
            pdr->effecter_id == effecterId)
        {
            foundPdr = true;
            states =
                reinterpret_cast<state_effecter_or_sensor_possible_states*>(
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

    if (foundPdr)
    {
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
                         auto bus = sdbusplus::bus::new_default();
                         try
                         {
                             auto service = getService(bus, objPath.c_str(),
                                                       dbusInterface);
                             auto method = bus.new_method_call(
                                 service.c_str(), objPath.c_str(),
                                 dbusProperties, "Set");
                             method.append(dbusInterface, dbusProp, value);
                             auto reply = bus.call(method);
                         }
                         catch (std::exception& e)
                         {
                             log<level::ERR>(
                                 "Error setting property",
                                 entry("PROPERTY=%s", dbusProp.c_str()),
                                 entry("INTERFACE=%s", dbusInterface.c_str()));
                             rc = PLDM_ERROR;
                             break;
                         }
                         break;
                     }
                     else if (stateSet.first == PLDM_SYSTEM_POWER_STATE)
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
                         auto bus = sdbusplus::bus::new_default();
                         try
                         {
                             auto service = getService(bus, objPath.c_str(),
                                                       dbusInterface);
                             auto method = bus.new_method_call(
                                 service.c_str(), objPath.c_str(),
                                 dbusProperties, "Set");
                             method.append(dbusInterface, dbusProp, value);
                             auto reply = bus.call(method);
                         }
                         catch (std::exception& e)
                         {
                             log<level::ERR>(
                                 "Error setting property",
                                 entry("PROPERTY=%s", dbusProp.c_str()),
                                 entry("INTERFACE=%s", dbusInterface.c_str()));
                             rc = PLDM_ERROR;
                             break;
                         }
                         break;
                     }
                 }
             }}};

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
            } // end inner for
            states =
                reinterpret_cast<state_effecter_or_sensor_possible_states*>(
                    pdr->possible_states +
                    sizeof(state_effecter_or_sensor_possible_states));
        } // end outer for
    }     // end if foundPdr

    else
    {
        log<level::ERR>("PDR not found", entry("EFFECTER ID=%d", effecterId));
        rc = PLDM_ERROR_INVALID_DATA;
    }

    return rc;
}

} // namespace responder
} // namespace pldm
