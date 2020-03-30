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

/** @class EventHandler
 *
 *  @brief
 */
class EventHandler
{
  public:
    EventHandler() = delete;
    explicit EventHandler(const std::string& dirPath);
    virtual ~EventHandler() = default;
    EventHandler(const EventHandler&) = default;
    EventHandler& operator=(const EventHandler&) = default;
    EventHandler(EventHandler&&) = default;
    EventHandler& operator=(EventHandler&&) = default;

    int eventAction(const EventEntry& entry, EventState state);

    const EventDBusInfo& getEventInfo(const EventEntry&);

    StateToDBusValue mapStateToDBusVal(const Json& eventStates,
                                       const Json& propertyValues,
                                       std::string_view type);

  private:
    EventMap eventMap;
};

} // namespace pldm::responder::events