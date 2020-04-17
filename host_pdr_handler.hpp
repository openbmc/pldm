#pragma once

#include "dbus_impl_requester.hpp"
#include "libpldmresponder/pdr.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <memory>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>
#include <set>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/platform.h"

using namespace pldm::dbus_api;

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
                   pldm_pdr* repo, Requester& requester) :
        mctp_fd(mctp_fd),
        mctp_eid(mctp_eid), event(event), repo(repo), requester(requester)
    {
    }

    /**@brief fetch remote PDRs based on the recordHandles
     *@param[in] recordHandles - a vector of recordHandles
     */

    void fetchPDR(std::vector<uint32_t>&& recordHandles);

    /**brief preparepldmPDRRepositoryChgEventData based on the pdrTypes
     *@param[in] pdrTypes - a vector of PDR Types
     *@param[in] eventDataFormat - format of this eventData
     *@param[in] repo - opaque pointer acting as PDR repo handle
     */
    std::vector<uint8_t> preparepldmPDRRepositoryChgEventData(
        const std::vector<uint8_t>& pdrTypes, uint8_t eventDataFormat,
        const pldm_pdr* repo);

    /**brief sendpldmPDRRepositoryChgEventData based on the eventData
     *@param[in] eventData - The event data
     *@param[in] mctp_eid - MCTP endpoint id
     *@param[in] fd - MCTP socket fd
     */
    int sendpldmPDRRepositoryChgEventData(const std::vector<uint8_t> eventData,
                                          uint8_t mctp_eid, int fd, Requester&);

    /** @brief Parse the State Sensor PDR's in the host PDR repository and build
     *         the HostStateSensorMap data structure which will be used to
     *         lookup the sensor info in the PLatformEventMessage command.
     *
     *  @param[in] repo - opaque pointer acting as PDR repo handle
     */
    void parseStateSensorPDRs(pldm_pdr* repo);

    /** @brief Lookup host sensor info corresponding to requested SensorEntry
     *
     *  @param[in] entry - TerminusID and SensorID
     *
     *  @return SensorInfo corresponding to the input paramter SensorEntry
     *          throw std::out_of_range exception if not found
     */
    const SensorInfo& lookupSensorInfo(const SensorEntry& entry)
    {
        return sensorMap.at(entry);
    }

  private:
    void _fetchPDR(sdeventplus::source::EventBase& source);

    int mctp_fd;
    uint8_t mctp_eid;
    sdeventplus::Event& event;
    pldm_pdr* repo;
    Requester& requester;
    std::unique_ptr<sdeventplus::source::Defer> pdrFetcherEventSrc;
    PDRRecordHandles pdrRecordHandles;
    // sensorMap is a lookup data structure that is build from the hostPDR
    // that speeds up the lookup of <TerminusID, SensorID> in
    // PlatformEventMessage command request.
    HostStateSensorMap sensorMap;
};

} // namespace pldm
