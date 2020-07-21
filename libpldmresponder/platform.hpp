#pragma once

#include "config.h"

#include "libpldm/platform.h"
#include "libpldm/states.h"

#include "common/utils.hpp"
#include "event_parser.hpp"
#include "fru.hpp"
#include "host-bmc/host_pdr_handler.hpp"
#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "pldmd/handler.hpp"

#include <stdint.h>

#include <map>

namespace pldm
{
namespace responder
{
namespace platform
{

using namespace pldm::utils;
using namespace pldm::responder::pdr_utils;

using generatePDR =
    std::function<void(const pldm::utils::DBusHandler& dBusIntf,
                       const Json& json, pdr_utils::RepoInterface& repo)>;

using EffecterId = uint16_t;
using DbusObjMaps =
    std::map<EffecterId,
             std::tuple<pdr_utils::DbusMappings, pdr_utils::DbusValMaps>>;
using DbusPath = std::string;
using EffecterObjs = std::vector<DbusPath>;
using EventType = uint8_t;
using EventHandler = std::function<int(
    const pldm_msg* request, size_t payloadLength, uint8_t formatVersion,
    uint8_t tid, size_t eventDataOffset)>;
using EventHandlers = std::vector<EventHandler>;
using EventMap = std::map<EventType, EventHandlers>;

// EventEntry = <uint8_t> - EventState <uint8_t> - SensorOffset <uint16_t> -
// SensorID
using EventEntry = uint32_t;
struct DBusInfo
{
    pldm::utils::DBusMapping dBusValues;
    pldm::utils::PropertyValue dBusPropertyValue;
};

class Handler : public CmdHandler
{
  public:
    Handler(const pldm::utils::DBusHandler* dBusIntf,
            const std::string& pdrJsonsDir, const std::string& eventsJsonsDir,
            pldm_pdr* repo, HostPDRHandler* hostPDRHandler,
            fru::Handler* fruHandler, bool buildPDRLazily = false,
            const std::optional<EventMap>& addOnHandlersMap = std::nullopt) :
        pdrRepo(repo),
        hostPDRHandler(hostPDRHandler), stateSensorHandler(eventsJsonsDir),
        fruHandler(fruHandler), dBusIntf(dBusIntf), pdrJsonsDir(pdrJsonsDir),
        pdrCreated(false)
    {
        if (!buildPDRLazily)
        {
            generate(*dBusIntf, pdrJsonsDir, pdrRepo);
            pdrCreated = true;
        }

        handlers.emplace(PLDM_GET_PDR,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->getPDR(request, payloadLength);
                         });
        handlers.emplace(PLDM_SET_NUMERIC_EFFECTER_VALUE,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->setNumericEffecterValue(
                                 request, payloadLength);
                         });
        handlers.emplace(PLDM_SET_STATE_EFFECTER_STATES,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->setStateEffecterStates(request,
                                                                 payloadLength);
                         });
        handlers.emplace(PLDM_PLATFORM_EVENT_MESSAGE,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->platformEventMessage(request,
                                                               payloadLength);
                         });

        // Default handler for PLDM Events
        eventHandlers[PLDM_SENSOR_EVENT].emplace_back(
            [this](const pldm_msg* request, size_t payloadLength,
                   uint8_t formatVersion, uint8_t tid, size_t eventDataOffset) {
                return this->sensorEvent(request, payloadLength, formatVersion,
                                         tid, eventDataOffset);
            });
        eventHandlers[PLDM_PDR_REPOSITORY_CHG_EVENT].emplace_back(
            [this](const pldm_msg* request, size_t payloadLength,
                   uint8_t formatVersion, uint8_t tid, size_t eventDataOffset) {
                return this->pldmPDRRepositoryChgEvent(request, payloadLength,
                                                       formatVersion, tid,
                                                       eventDataOffset);
            });

        // Additional OEM event handlers for PLDM events, append it to the
        // standard handlers
        if (addOnHandlersMap)
        {
            auto addOnHandlers = addOnHandlersMap.value();
            for (EventMap::iterator iter = addOnHandlers.begin();
                 iter != addOnHandlers.end(); ++iter)
            {
                auto search = eventHandlers.find(iter->first);
                if (search != eventHandlers.end())
                {
                    search->second.insert(std::end(search->second),
                                          std::begin(iter->second),
                                          std::end(iter->second));
                }
                else
                {
                    eventHandlers.emplace(iter->first, iter->second);
                }
            }
        }
    }

    pdr_utils::Repo& getRepo()
    {
        return this->pdrRepo;
    }

    /** @brief Add D-Bus mapping and value mapping(stateId to D-Bus) for the
     *         effecterId. If the same id is added, the previous dbusObjs will
     *         be "over-written".
     *
     *  @param[in] effecterId - effecter id
     *  @param[in] dbusObj - list of D-Bus object structure and list of D-Bus
     *                       property value to attribute value
     */
    void addDbusObjMaps(
        uint16_t effecterId,
        std::tuple<pdr_utils::DbusMappings, pdr_utils::DbusValMaps> dbusObj);

    /** @brief Retrieve an effecter id -> D-Bus objects mapping
     *
     *  @param[in] effecterId - effecter id
     *
     *  @return std::tuple<pdr_utils::DbusMappings, pdr_utils::DbusValMaps> -
     *          list of D-Bus object structure and list of D-Bus property value
     *          to attribute value
     */
    const std::tuple<pdr_utils::DbusMappings, pdr_utils::DbusValMaps>&
        getDbusObjMaps(uint16_t effecterId) const;

    uint16_t getNextEffecterId()
    {
        return ++nextEffecterId;
    }

    /** @brief Parse PDR JSONs and build PDR repository
     *
     *  @param[in] dBusIntf - The interface object
     *  @param[in] dir - directory housing platform specific PDR JSON files
     *  @param[in] repo - instance of concrete implementation of Repo
     */
    void generate(const pldm::utils::DBusHandler& dBusIntf,
                  const std::string& dir, Repo& repo);

    /** @brief Parse PDR JSONs and build state effecter PDR repository
     *
     *  @param[in] json - platform specific PDR JSON files
     *  @param[in] repo - instance of state effecter implementation of Repo
     */
    void generateStateEffecterRepo(const Json& json, Repo& repo);

    /** @brief map of PLDM event type to EventHandlers
     *
     */
    EventMap eventHandlers;

    /** @brief Handler for GetPDR
     *
     *  @param[in] request - Request message payload
     *  @param[in] payloadLength - Request payload length
     *  @param[out] Response - Response message written here
     */
    Response getPDR(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for setNumericEffecterValue
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @return Response - PLDM Response message
     */
    Response setNumericEffecterValue(const pldm_msg* request,
                                     size_t payloadLength);

    /** @brief Handler for setStateEffecterStates
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @return Response - PLDM Response message
     */
    Response setStateEffecterStates(const pldm_msg* request,
                                    size_t payloadLength);

    /** @brief Handler for PlatformEventMessage
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @return Response - PLDM Response message
     */
    Response platformEventMessage(const pldm_msg* request,
                                  size_t payloadLength);

    /** @brief Handler for event class Sensor event
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @param[in] formatVersion - Version of the event format
     *  @param[in] tid - Terminus ID of the event's originator
     *  @param[in] eventDataOffset - Offset of the event data in the request
     *                               message
     *  @return PLDM completion code
     */
    int sensorEvent(const pldm_msg* request, size_t payloadLength,
                    uint8_t formatVersion, uint8_t tid, size_t eventDataOffset);

    /** @brief Handler for pldmPDRRepositoryChgEvent
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @param[in] formatVersion - Version of the event format
     *  @param[in] tid - Terminus ID of the event's originator
     *  @param[in] eventDataOffset - Offset of the event data in the request
     *                               message
     *  @return PLDM completion code
     */
    int pldmPDRRepositoryChgEvent(const pldm_msg* request, size_t payloadLength,
                                  uint8_t formatVersion, uint8_t tid,
                                  size_t eventDataOffset);

    /** @brief Handler for extracting the PDR handles from changeEntries
     *
     *  @param[in] changeEntryData - ChangeEntry data from changeRecord
     *  @param[in] changeEntryDataSize - total size of changeEntryData
     *  @param[in] numberOfChangeEntries - total number of changeEntries to
     *                                     extract
     *  @param[out] pdrRecordHandles - std::vector where the extracted PDR
     *                                 handles are placed
     *  @return PLDM completion code
     */
    int getPDRRecordHandles(const ChangeEntry* changeEntryData,
                            size_t changeEntryDataSize,
                            size_t numberOfChangeEntries,
                            PDRRecordHandles& pdrRecordHandles);

    /** @brief Handler for setting Sensor event data
     *
     *  @param[in] sensorId - sensorID value of the sensor
     *  @param[in] sensorOffset - Identifies which state sensor within a
     * composite state sensor the event is being returned for
     *  @param[in] eventState - The event state value from the state change that
     * triggered the event message
     *  @return PLDM completion code
     */
    int setSensorEventData(uint16_t sensorId, uint8_t sensorOffset,
                           uint8_t eventState);

  private:
    pdr_utils::Repo pdrRepo;
    uint16_t nextEffecterId{};
    DbusObjMaps dbusObjMaps{};
    HostPDRHandler* hostPDRHandler;
    events::StateSensorHandler stateSensorHandler;
    fru::Handler* fruHandler;
    const pldm::utils::DBusHandler* dBusIntf;
    std::string pdrJsonsDir;
    bool pdrCreated;
};

} // namespace platform
} // namespace responder
} // namespace pldm
