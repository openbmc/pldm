#pragma once

#include "common/endpoint.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "requester/endpoint_registry.hpp"

#include <libpldm/pldm.h>

#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Common/UUID/common.hpp>
#include <xyz/openbmc_project/MCTP/Endpoint/client.hpp>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <set>
#include <utility>
#include <vector>

using MCTPEndpoint = sdbusplus::common::xyz::openbmc_project::mctp::Endpoint;
using CommonUUID = sdbusplus::common::xyz::openbmc_project::common::UUID;

class TestMctpDiscovery;

namespace pldm
{

const std::string emptyUUID = "00000000-0000-0000-0000-000000000000";
constexpr const char* MCTPService = "au.com.codeconstruct.MCTP1";
constexpr const char* MCTPPath = "/au/com/codeconstruct/mctp1";
constexpr const char* MCTPInterfaceCC = "au.com.codeconstruct.MCTP.Endpoint1";
constexpr const char* MCTPConnectivityProp = "Connectivity";
constexpr const char* inventorySubtreePathStr =
    "/xyz/openbmc_project/inventory/system";

const std::vector<std::string> interfaceFilter = {
    "xyz.openbmc_project.Configuration.MCTPI2CTarget",
    "xyz.openbmc_project.Configuration.MCTPI3CTarget",
    "xyz.openbmc_project.Configuration.MCTPUSBDevice"};

/** @brief An MctpInfo paired with the D-Bus object path of the MCTP endpoint it
 *         was discovered at. The object path identifies the endpoint: it is
 *         the key used to avoid adding the same endpoint twice, to find a
 *         tracked endpoint when its Connectivity changes and to find it when
 *         its D-Bus object is removed. Entries are matched by comparing the
 *         path as an opaque string, its layout is not parsed.
 */
using MctpInfoWithPath = std::pair<dbus::ObjectPath, MctpInfo>;
using MctpInfosWithPath = std::vector<MctpInfoWithPath>;

/** @class MctpDiscoveryHandlerIntf
 *
 * This abstract class defines the APIs for MctpDiscovery class has common
 * interface to execute function from different Manager Classes
 */
class MctpDiscoveryHandlerIntf
{
  public:
    virtual void handleMctpEndpoints(const MctpInfos& mctpInfos) = 0;
    virtual void handleRemovedMctpEndpoints(const MctpInfos& mctpInfos) = 0;

    /** @brief Handle added endpoints together with their PeerIds.
     *
     *  This is what MctpDiscovery calls. By default it drops the PeerIds and
     *  calls handleMctpEndpoints(), so handlers that do not need the PeerId
     *  need not change.
     *
     *  @param[in] endpoints - the added endpoints
     */
    virtual void handleEndpoints(
        const std::vector<DiscoveredEndpoint>& endpoints)
    {
        handleMctpEndpoints(toMctpInfos(endpoints));
    }

    /** @brief Handle removed endpoints together with their PeerIds.
     *
     *  The PeerIds are still valid while this is called. By default it drops
     *  them and calls handleRemovedMctpEndpoints().
     *
     *  @param[in] endpoints - the removed endpoints
     */
    virtual void handleRemovedEndpoints(
        const std::vector<DiscoveredEndpoint>& endpoints)
    {
        handleRemovedMctpEndpoints(toMctpInfos(endpoints));
    }

    virtual void updateMctpEndpointAvailability(const MctpInfo& mctpInfo,
                                                Availability availability) = 0;
    /** @brief Get the PeerId of the active endpoint of a terminus.
     *
     *  @param[in] terminusName - MCTP terminus name
     *
     *  @return the PeerId that addresses the endpoint in the transport,
     *          std::nullopt if there is no active endpoint of that name
     */
    virtual std::optional<PeerId> getActivePeerIdByName(
        const std::string& terminusName) = 0;

    virtual void handleConfigurations(const Configurations& /*configurations*/)
    {}
    virtual ~MctpDiscoveryHandlerIntf() {}

  private:
    static MctpInfos toMctpInfos(
        const std::vector<DiscoveredEndpoint>& endpoints)
    {
        MctpInfos mctpInfos;
        for (const auto& endpoint : endpoints)
        {
            mctpInfos.emplace_back(endpoint.mctpInfo);
        }
        return mctpInfos;
    }
};

/** @brief Tells whether an endpoint answers a PLDM request
 *
 *  It is called with the PeerId of the endpoint and a function that it has to
 *  call, once, with true if the endpoint answered and with false if it did not.
 */
using ReachabilityCheck =
    std::function<void(PeerId, std::function<void(bool)>)>;

class MctpDiscovery
{
  public:
    MctpDiscovery() = delete;
    MctpDiscovery(const MctpDiscovery&) = delete;
    MctpDiscovery(MctpDiscovery&&) = delete;
    MctpDiscovery& operator=(const MctpDiscovery&) = delete;
    MctpDiscovery& operator=(MctpDiscovery&&) = delete;
    ~MctpDiscovery() = default;

    /** @brief Constructs the MCTP Discovery object to handle discovery of
     *         MCTP enabled devices
     *
     *  @param[in] bus - reference to systemd bus
     *  @param[in] list - initializer list to the MctpDiscoveryHandlerIntf
     *  @param[in] registry - assigns the PeerId of each discovered endpoint.
     *                        When null, MctpDiscovery uses its own registry
     *                        with PeerId equal to the EID.
     *  @param[in] groupByUuid - treat the endpoints that have the same UUID as
     *                        one device that is reachable more than once, e.g.
     *                        over more than one MCTP Network. Only the one
     *                        that was discovered first is given to the
     *                        handlers, the others stand by. When that one is
     *                        removed, an available one that stands by takes its
     *                        place, as a new device. One that is not available
     *                        keeps standing by until it is. A loss of
     *                        connectivity does not switch to another one. An
     *                        empty UUID never groups.
     *  @param[in] reachabilityCheck - when set, an endpoint of a device that is
     *                        grouped by UUID is only given to the handlers once
     *                        it answers. One that does not is put back on
     *                        standby and the next one is tried, until one
     *                        answers or none is left. One that did not answer
     *                        is tried again when it is available again.
     */
    explicit MctpDiscovery(
        sdbusplus::bus_t& bus,
        std::initializer_list<MctpDiscoveryHandlerIntf*> list,
        EndpointRegistry* registry = nullptr, bool groupByUuid = false,
        ReachabilityCheck reachabilityCheck = nullptr);

    /** @brief reference to the systemd bus */
    sdbusplus::bus_t& bus;

    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::match mctpEndpointAddedSignal;

    /** @brief Used to watch for the removed MCTP endpoints */
    sdbusplus::match mctpEndpointRemovedSignal;

    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::match mctpEndpointPropChangedSignal;

    /** @brief List of handlers need to notify when new MCTP
     * Endpoint is Added/Removed */
    std::vector<MctpDiscoveryHandlerIntf*> handlers;

    /** @brief Registry used when the creator does not provide one */
    std::unique_ptr<EndpointRegistry> ownedRegistry;

    /** @brief Assigns the PeerId of each endpoint handed to the handlers */
    EndpointRegistry& registry;

    /** @brief Whether the endpoints with the same UUID are one device */
    bool groupByUuid;

    /** @brief Tells whether an endpoint answers, when it has to be checked
     *         before it is given to the handlers
     */
    ReachabilityCheck reachabilityCheck;

    /** @brief The object path of the endpoint of each UUID that is given to
     *         the handlers, or is being checked, when grouping by UUID
     */
    std::map<UUID, dbus::ObjectPath> activeByUuid;

    /** @brief The object paths of the endpoints that stand by for the active
     *         endpoint of their UUID, when grouping by UUID
     */
    std::set<dbus::ObjectPath> standby;

    /** @brief The object paths of the endpoints that are not available, so that
     *         only an endpoint that can be reached takes over
     */
    std::set<dbus::ObjectPath> unavailable;

    /** @brief The endpoints that are being checked, and the number of the
     *         check, so that the answer to an old check is ignored
     */
    std::map<dbus::ObjectPath, uint64_t> checking;

    /** @brief The number of the last check */
    uint64_t lastCheck = 0;

    /** @brief The endpoints that did not answer. They stand by and are not
     *         tried again until they are available again.
     */
    std::set<dbus::ObjectPath> notAnswering;

    /** @brief The existing MCTP endpoints and their D-Bus object paths */
    MctpInfosWithPath existingMctpInfos;

    /** @brief Callback function when the propertiesChanged D-Bus
     * signal is triggered for MCTP endpoint's properties.
     *
     *  @param[in] msg - Data associated with subscribed signal
     */
    void propertiesChangedCb(sdbusplus::message_t& msg);

    /** @brief Callback function when MCTP endpoints addedInterface
     * D-Bus signal raised.
     *
     *  @param[in] msg - Data associated with subscribed signal
     */
    void discoverEndpoints(sdbusplus::message_t& msg);

    /** @brief Callback function when MCTP endpoint removedInterface
     * D-Bus signal raised.
     *
     *  @param[in] msg - Data associated with subscribed signal
     */
    void removeEndpoints(sdbusplus::message_t& msg);

    /** @brief Notify the registered handlers about the added MCTP endpoints.
     *  Each endpoint is given a PeerId first. An endpoint that cannot get one
     *  is logged and skipped.
     *
     *  @param[in] endpoints - added MCTP endpoints, each with the D-Bus
     *  object path it was discovered at
     */
    void handleMctpEndpoints(const MctpInfosWithPath& endpoints);

    /** @brief Helper function to invoke registered handlers for
     *  the removed MCTP endpoints. The PeerIds of the endpoints are released
     *  after the handlers ran.
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    void handleRemovedMctpEndpoints(const MctpInfos& mctpInfos);

    /** @brief Helper function to invoke registered handlers for
     *  updating the availability status of the MCTP endpoint
     *
     *  @param[in] mctpInfo - information of the target endpoint
     *  @param[in] availability - new availability status
     */
    void updateMctpEndpointAvailability(const MctpInfo& mctpInfo,
                                        Availability availability);

    /** @brief Get list of MctpInfos in MCTP control interface.
     *
     *  @param[out] mctpInfoMap - information of discovered MCTP endpoints
     *  and the D-Bus object path and availability status of each endpoint
     */
    void getMctpInfos(
        std::map<MctpInfo, std::pair<dbus::ObjectPath, Availability>>&
            mctpInfoMap);

    /** @brief Get list of new MctpInfos in addedInterace D-Bus signal message.
     *
     *  @param[in] msg - addedInterace D-Bus signal message
     *  @param[out] mctpInfos - information and D-Bus object paths of added MCTP
     *  endpoints
     */
    void getAddedMctpInfos(sdbusplus::message_t& msg,
                           MctpInfosWithPath& mctpInfos);

    /** @brief Add new MCTP endpoints to existingMctpInfos. An endpoint whose
     *  D-Bus object path is already in existingMctpInfos is not added again.
     *
     *  @param[in] mctpInfos - information and D-Bus object paths of new MCTP
     *  endpoints
     */
    void addToExistingMctpInfos(const MctpInfosWithPath& mctpInfos);

    friend class ::TestMctpDiscovery;

  private:
    /** @brief Decide whether an endpoint stands by for another one with the
     *         same UUID instead of being given to the handlers. If it does it
     *         is remembered as standing by.
     *
     *  @param[in] path - the D-Bus object path of the endpoint
     *  @param[in] mctpInfo - information of the endpoint
     *
     *  @return true if the endpoint stands by
     */
    bool standsBy(const dbus::ObjectPath& path, const MctpInfo& mctpInfo);

    /** @brief Give the handlers the first endpoint, in the order of discovery,
     *         that stands by for the active endpoint of a UUID, is available,
     *         did not fail to answer and can get a PeerId. Used when the active
     *         endpoint is removed or does not answer, and when one that stands
     *         by becomes available and none is active.
     *
     *  @param[in] uuid - the UUID of the device
     */
    void promoteStandby(const UUID& uuid);

    /** @brief Give the added endpoints to the handlers
     *
     *  @param[in] discovered - the endpoints and their PeerIds
     */
    void giveToHandlers(const std::vector<DiscoveredEndpoint>& discovered);

    /** @brief Start to check that an endpoint answers, before it is given to
     *         the handlers
     *
     *  @param[in] path - the D-Bus object path of the endpoint
     *  @param[in] mctpInfo - information of the endpoint
     *  @param[in] peer - the PeerId of the endpoint
     */
    void checkReachable(const dbus::ObjectPath& path, const MctpInfo& mctpInfo,
                        PeerId peer);

    /** @brief The endpoint was checked: give it to the handlers if it answered,
     *         otherwise put it back on standby and try the next one
     *
     *  @param[in] path - the D-Bus object path of the endpoint
     *  @param[in] mctpInfo - information of the endpoint
     *  @param[in] peer - the PeerId of the endpoint
     *  @param[in] check - the number of the check, ignored if it is not the
     *                     last one of the endpoint
     *  @param[in] answered - whether the endpoint answered
     */
    void checked(const dbus::ObjectPath& path, const MctpInfo& mctpInfo,
                 PeerId peer, uint64_t check, bool answered);

    /** @brief Get MCTP Endpoint D-Bus Properties in the
     *         `xyz.openbmc_project.MCTP.Endpoint` D-Bus interface
     *
     *  @param[in] service - the MCTP service name
     *  @param[in] path - the MCTP endpoints object path
     *
     *  @return tuple of Network Index, Endpoint ID and MCTP message types
     */
    MctpEndpointProps getMctpEndpointProps(const std::string& service,
                                           const std::string& path);

    /** @brief Get Endpoint UUID from `UUID` D-Bus property in the
     *         `xyz.openbmc_project.Common.UUID` D-Bus interface.
     *
     *  @param[in] service - the MCTP service name
     *  @param[in] path - the MCTP endpoints object path
     *
     *  @return Endpoint UUID
     */
    UUID getEndpointUUIDProp(const std::string& service,
                             const std::string& path);

    /** @brief Get Endpoint Availability status from `Connectivity` D-Bus
     *         property in the `au.com.codeconstruct.MCTP.Endpoint1` D-Bus
     *         interface.
     *
     *  @param[in] path - the MCTP endpoints object path
     *
     *  @return Availability status: true if active false if inactive
     */
    Availability getEndpointConnectivityProp(const std::string& path);

    static constexpr uint8_t mctpTypePLDM = 1;

    /** @brief Construct the MCTP reactor object path
     *
     *  @param[in] mctpInfo - information of discovered MCTP endpoint
     *
     *  @return the MCTP reactor object path
     */
    std::string constructMctpReactorObjectPath(const MctpInfo& mctpInfo);

    /** @brief Search for associated configuration for the MctpInfo.
     *
     *  @param[in] mctpInfo - information of discovered MCTP endpoint
     */
    void searchConfigurationFor(const pldm::utils::DBusHandler& handler,
                                MctpInfo& mctpInfo);

    /** @brief Remove configuration associated with the removed MCTP endpoint.
     *
     *  @param[in] removedInfos - the removed MCTP endpoints
     */
    void removeConfigs(const MctpInfos& removedInfos);

    /** @brief An internal helper function to get the name property from the
     * properties
     * @param[in] properties - the properties of the D-Bus object
     * @return the name property
     */
    std::string getNameFromProperties(const utils::PropertyMap& properties);

    /** @brief The configuration contains D-Bus path and the MCTP endpoint
     * information.
     */
    Configurations configurations;
};

} // namespace pldm
