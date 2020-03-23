#pragma once

#include "utils.hpp"

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <tuple>
#include <vector>

namespace pldm::responder::events
{

/** @struct EventEntry
 *
 *  EventEntry is a unique key which is used to map a PlatformEventMessage
 *  sensorEvent to the BMC D-Bus property. This struct is used as a key in
 *  a std::map so implemented opertor== and operator<.
 */
struct EventEntry
{
    uint16_t containerId;
    uint16_t entityType;
    uint16_t entityInstance;
    uint8_t sensorOffset;

    bool operator==(const EventEntry& e) const
    {
        return ((containerId == e.containerId) &&
                (entityType == e.entityType) &&
                (entityInstance == e.entityInstance) &&
                (sensorOffset == e.sensorOffset));
    }

    bool operator<(const EventEntry& e) const
    {
        return (
            (containerId < e.containerId) ||
            ((containerId == e.containerId) && (entityType < e.entityType)) ||
            ((containerId == e.containerId) && (entityType == e.entityType) &&
             (entityInstance < e.entityInstance)) ||
            ((containerId == e.containerId) && (entityType == e.entityType) &&
             (entityInstance == e.entityInstance) &&
             (sensorOffset < e.sensorOffset)));
    }
};

using EventState = uint8_t;
using StateToDBusValue = std::map<EventState, pldm::utils::PropertyValue>;
using EventDBusInfo = std::tuple<pldm::utils::DBusMapping, StateToDBusValue>;
using EventMap = std::map<EventEntry, EventDBusInfo>;
using Json = nlohmann::json;

/** @class EventParser
 *
 *  @brief Parses the event state sensor configuration JSON files to populate
 *         the data structure, containing the information needed to map the
 *         event state to the D-Bus property. Provide an API that abstracts the
 *         action to be taken for a platform event message.
 */
class EventParser
{
  public:
    EventParser() = delete;
    explicit EventParser(const std::string& dirPath);
    virtual ~EventParser() = default;
    EventParser(const EventParser&) = default;
    EventParser& operator=(const EventParser&) = default;
    EventParser(EventParser&&) = default;
    EventParser& operator=(EventParser&&) = default;

    /** @brief Invokes the action for the EventEntry and EventState attributed
     *         to the sensor. If the EventEntry and EventState is valid, the
     *         D-Bus property corresponding to the EventEntry is set based on
     *         the EventState
     *
     *  @param[in] entry - event entry
     *  @param[in] state - event state
     *
     *  @return PLDM completion code
     */
    int eventAction(const EventEntry& entry, EventState state);

    /** @brief Helper API to get D-Bus information for an EventEntry
     *
     *  @param[in] entry - event entry
     *
     *  @return D-Bus information corresponding to the EventEntry
     */
    const EventDBusInfo& getEventInfo(const EventEntry& entry) const
    {
        return eventMap.at(entry);
    }

  private:
    EventMap eventMap; //!< a map of EventEntry to D-Bus information

    /** @brief Create a map of EventState to D-Bus property values from
     *         the information provided in the event state configuration
     *         JSON
     *
     *  @param[in] eventStates - a JSON array of event states
     *  @param[in] propertyValues - a JSON array of D-Bus property values
     *  @param[in] type - the type of D-Bus property
     *
     *  @return a map of EventState to D-Bus property values
     */
    StateToDBusValue mapStateToDBusVal(const Json& eventStates,
                                       const Json& propertyValues,
                                       std::string_view type);
};

} // namespace pldm::responder::events