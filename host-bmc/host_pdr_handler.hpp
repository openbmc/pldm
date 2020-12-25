#pragma once

#include "libpldm/base.h"
#include "libpldm/platform.h"

#include "common/types.hpp"
#include "common/utils.hpp"
#include "libpldmresponder/event_parser.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "pldmd/dbus_impl_requester.hpp"

#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <deque>
#include <map>
#include <memory>
#include <vector>

using namespace pldm::dbus_api;
using namespace pldm::responder::events;

namespace pldm
{

using ObjectPath = std::string;
using EntityName = std::string;
using EntityType = uint16_t;
// vector which would hold the PDR record handle data returned by
// pldmPDRRepositoryChgEvent event data
using ChangeEntry = uint32_t;
using PDRRecordHandles = std::deque<ChangeEntry>;

const std::map<EntityType, EntityName> entityMap = {
    {45, "chassis"}, {60, "io_board"}, {64, "motherboard"},
    {66, "dimm"},    {67, "cpu"},      {120, "powersupply"}};

/** @struct SensorEntry
 *
 *  SensorEntry is a unique key which maps a sensorEventType request in the
 *  PlatformEventMessage command to a host sensor PDR. This struct is a key
 *  in a std::map, so implemented operator==and operator<.
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

using HostStateSensorMap = std::map<SensorEntry, pdr::SensorInfo>;
using PDRList = std::vector<std::vector<uint8_t>>;

/** @class HostPDRHandler
 *  @brief This class can fetch and process PDRs from host firmware
 *  @details Provides an API to fetch PDRs from the host firmware. Upon
 *  receiving the PDRs, they are stored into the BMC's primary PDR repo.
 *  Adjustments are made to entity association PDRs received from the host,
 *  because they need to be assimilated into the BMC's entity association
 *  tree. A PLDM event containing the record handles of the updated entity
 *  association PDRs is sent to the host.
 */
class HostPDRHandler
{
  public:
    HostPDRHandler() = delete;
    HostPDRHandler(const HostPDRHandler&) = delete;
    HostPDRHandler(HostPDRHandler&&) = delete;
    HostPDRHandler& operator=(const HostPDRHandler&) = delete;
    HostPDRHandler& operator=(HostPDRHandler&&) = delete;
    ~HostPDRHandler() = default;

    using TLPDRMap = std::map<pdr::TerminusHandle, pdr::TerminusID>;

    /** @brief Constructor
     *  @param[in] mctp_fd - fd of MCTP communications socket
     *  @param[in] mctp_eid - MCTP EID of host firmware
     *  @param[in] event - reference of main event loop of pldmd
     *  @param[in] repo - pointer to BMC's primary PDR repo
     *  @param[in] eventsJsonDir - directory path which has the config JSONs
     *  @param[in] tree - pointer to BMC's entity association tree
     *  @param[in] requester - reference to Requester object
     */
    explicit HostPDRHandler(int mctp_fd, uint8_t mctp_eid,
                            sdeventplus::Event& event, pldm_pdr* repo,
                            const std::string& eventsJsonsDir,
                            pldm_entity_association_tree* entityTree,
                            Requester& requester);

    /** @brief fetch PDRs from host firmware. See @class.
     *  @param[in] recordHandles - list of record handles pointing to host's
     *             PDRs that need to be fetched.
     */

    void fetchPDR(PDRRecordHandles&& recordHandles);

    /** @brief Send a PLDM event to host firmware containing a list of record
     *  handles of PDRs that the host firmware has to fetch.
     *  @param[in] pdrTypes - list of PDR types that need to be looked up in the
     *                        BMC repo
     *  @param[in] eventDataFormat - format for PDRRepositoryChgEvent in DSP0248
     */
    void sendPDRRepositoryChgEvent(std::vector<uint8_t>&& pdrTypes,
                                   uint8_t eventDataFormat);

    /** @brief Lookup host sensor info corresponding to requested SensorEntry
     *
     *  @param[in] entry - TerminusID and SensorID
     *
     *  @return SensorInfo corresponding to the input paramter SensorEntry
     *          throw std::out_of_range exception if not found
     */
    const pdr::SensorInfo& lookupSensorInfo(const SensorEntry& entry) const
    {
        return sensorMap.at(entry);
    }

    /** @brief Handles state sensor event
     *
     *  @param[in] entry - state sensor entry
     *  @param[in] state - event state
     *
     *  @return PLDM completion code
     */
    int handleStateSensorEvent(const StateSensorEntry& entry,
                               pdr::EventState state);

    /** @brief Parse state sensor PDRs and populate the sensorMap lookup data
     *         structure
     *
     *  @param[in] stateSensorPDRs - host state sensor PDRs
     *  @param[in] tlpdrInfo - terminus locator PDRs info
     *
     */
    void parseStateSensorPDRs(const PDRList& stateSensorPDRs,
                              const TLPDRMap& tlpdrInfo);

    /** @brief Parse FRU record set PDRs
     *
     *  @param[in] fruRecordSetPDRs - host fru record set PDRs
     *
     */
    void parseFruRecordSetPDRs(const PDRList& fruRecordSetPDRs);

  private:
    /** @brief fetchPDR schedules work on the event loop, this method does the
     *  actual work. This is so that the PDR exchg with the host is async.
     *  @param[in] source - sdeventplus event source
     */
    void _fetchPDR(sdeventplus::source::EventBase& source);

    /** @brief Merge host firmware's entity association PDRs into BMC's
     *  @details A merge operation involves adding a pldm_entity under the
     *  appropriate parent, and updating container ids.
     *  @param[in] pdr - entity association pdr
     */
    void mergeEntityAssociations(const std::vector<uint8_t>& pdr);

    /** @brief Find parent of input entity type, from the entity association
     *  tree
     *  @param[in] type - PLDM entity type
     *  @param[out] parent - PLDM entity information of parent
     *  @return bool - true if parent found, false otherwise
     */
    bool getParent(EntityType type, pldm_entity& parent);

    /** @brief maps a entity name to pldm_entity from entity association tree
     *  @param[in] entityMaps       - maps an entity name to pldm_entity
     *  @param[in] path             - object path of D-Bus
     *  @return
     */
    void addObjectPathEntityAssociationMap(
        const std::map<std::string, pldm_entity>& entityMaps,
        std::string& path);

    /** @brief Get FRU record table metadata by host
     *
     *  @return uint16_t    - total table records
     */
    uint16_t getFRURecordTableMetadataByHost();

    /** @brief Get FRU record table by host
     *
     *  @return
     */
    void getFRURecordTableByHost();

    /** @brief Get FRU Record Set Identifier from FRU Record data Format
     *  @param[in] fruRecordSetPDRs - fru record set pdr
     *  @param[in] entity           - PLDM entity information
     *  @return
     */
    uint16_t getRSI(const PDRList& fruRecordSetPDRs, const pldm_entity& entity);

    /** @brief fd of MCTP communications socket */
    int mctp_fd;
    /** @brief MCTP EID of host firmware */
    uint8_t mctp_eid;
    /** @brief reference of main event loop of pldmd, primarily used to schedule
     *  work.
     */
    sdeventplus::Event& event;
    /** @brief pointer to BMC's primary PDR repo, host PDRs are added here */
    pldm_pdr* repo;

    StateSensorHandler stateSensorHandler;
    /** @brief Pointer to BMC's entity association tree */
    pldm_entity_association_tree* entityTree;
    /** @brief reference to Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    Requester& requester;
    /** @brief sdeventplus event source */
    std::unique_ptr<sdeventplus::source::Defer> pdrFetchEvent;
    /** @brief list of PDR record handles pointing to host's PDRs */
    PDRRecordHandles pdrRecordHandles;
    /** @brief maps an entity type to parent pldm_entity from the BMC's entity
     *  association tree
     */
    std::map<EntityType, pldm_entity> parents;
    /** @brief D-Bus property changed signal match */
    std::unique_ptr<sdbusplus::bus::match::match> hostOffMatch;

    /** @brief sensorMap is a lookup data structure that is build from the
     *         hostPDR that speeds up the lookup of <TerminusID, SensorID> in
     *         PlatformEventMessage command request.
     */
    HostStateSensorMap sensorMap;

    /** @brief maps an object path to pldm_entity from the BMC's entity
     *         association tree
     */
    std::map<ObjectPath, pldm_entity> objPathMap;

    /** @brief maps an entity name to map, maps to entity name to pldm_entity
     */
    std::map<EntityName, std::map<EntityName, pldm_entity>>
        entityAssociationMap;

    /** @brief the vector of FRU Record Data Format
     */
    std::vector<responder::pdr_utils::FruRecordDataFormat> fruRecordData;

    /** @brief Object path and entity association and is only loaded once
     */
    bool objPathEntityAssociation;
};

} // namespace pldm
