#pragma once

#include "dbus_impl_requester.hpp"
#include "libpldmresponder/pdr.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <map>
#include <memory>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>
#include <set>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/platform.h"

using namespace pldm::dbus_api;

using EntityType = uint16_t;
// vector which would hold the PDR record handle data returned by
// pldmPDRRepositoryChgEvent evend data
using ChangeEntry = uint32_t;
using PDRRecordHandles = std::vector<ChangeEntry>;

namespace pldm
{

/** @struct sensorEntry
 *
 *  SensorEntry is a unique key which maps a request in the PlatformEventMessage
 *  command to the host sensor PDR. This struct is a key in a std::map, so
 *  implemented operator==and operator<.
 */
struct SensorEntry
{
    pdr::TerminusID terminusID;
    pdr::SensorID sensorID;

    bool operator==(const SensorEntry& e) const
    {
        return ((terminusID == e.terminusID) && (sensorID == e.sensorID));
    }

    bool operator<(const SensorEntry& e) const
    {
        return ((terminusID < e.terminusID) ||
                ((terminusID == e.terminusID) && (sensorID < e.sensorID)));
    }
};

//!< Subset of the State Set that is supported by the state sensor
using PossibleStates = std::set<uint8_t>;
//!< Subset of the State Set that is supported by each sensor in a composite
//!< sensor
using CompositeSensorStates = std::vector<PossibleStates>;
using EntityInfo =
    std::tuple<pdr::ContainerID, pdr::EntityType, pdr::EntityInstance>;
using SensorInfo = std::tuple<EntityInfo, CompositeSensorStates>;
using HostStateSensorMap = std::map<SensorEntry, SensorInfo>;

class HostPDRHandler
{
  public:
    HostPDRHandler() = delete;
    HostPDRHandler(HostPDRHandler&) = delete;
    HostPDRHandler(HostPDRHandler&&) = delete;
    HostPDRHandler& operator=(const HostPDRHandler&) = delete;
    HostPDRHandler& operator=(HostPDRHandler&&) = delete;
    ~HostPDRHandler() = default;

    HostPDRHandler(int mctp_fd, uint8_t mctp_eid, sdeventplus::Event& event,
                   pldm_pdr* repo, pldm_entity_association_tree* entityTree,
                   Requester& requester);

    /**@brief fetch remote PDRs based on the recordHandles
     *@param[in] recordHandles - a vector of recordHandles
     */

    void fetchPDR(std::vector<uint32_t>&& recordHandles);

    void sendPDRRepositoryChgEvent(std::vector<uint8_t>&& pdrTypes,
                                   uint8_t eventDataFormat);

    /** @brief Lookup host sensor info corresponding to requested SensorEntry
     *
     *  @param[in] entry - TerminusID and SensorID
     *
     *  @return SensorInfo corresponding to the input paramter SensorEntry
     *          throw std::out_of_range exception if not found
     */
    const SensorInfo& lookupSensorInfo(const SensorEntry& entry) const
    {
        return sensorMap.at(entry);
    }

  private:
    void _fetchPDR(sdeventplus::source::EventBase& source);

    void mergeEntityAssociations(const std::vector<uint8_t>& pdr);

    bool getParent(EntityType type, pldm_entity& parent);

    /** @brief Parse the State Sensor PDR and update the HostStateSensorMap
     *         data structure which will be used to lookup the sensor info
     *         in the PlatformEventMessage command.
     *
     *  @param[in] pdr - state sensor PDR
     */
    void parseStateSensorPDR(const std::vector<uint8_t>& stateSensorPdr);

    int mctp_fd;
    uint8_t mctp_eid;
    sdeventplus::Event& event;
    pldm_pdr* repo;
    pldm_entity_association_tree* entityTree;
    Requester& requester;
    std::unique_ptr<sdeventplus::source::Defer> pdrFetcherEventSrc;
    PDRRecordHandles pdrRecordHandles;
    std::map<EntityType, pldm_entity> parents;
    // sensorMap is a lookup data structure that is build from the hostPDR
    // that speeds up the lookup of <TerminusID, SensorID> in
    // PlatformEventMessage command request.
    HostStateSensorMap sensorMap;
};

} // namespace pldm
