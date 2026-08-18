#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/pldm.h>

#include <sdbusplus/bus/match.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>
#include <xyz/openbmc_project/Common/UUID/common.hpp>
#include <xyz/openbmc_project/MCTP/Endpoint/client.hpp>

#include <chrono>
#include <initializer_list>
#include <set>
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
    virtual void updateMctpEndpointAvailability(const MctpInfo& mctpInfo,
                                                Availability availability) = 0;
    /** @brief Get Active EIDs.
     *
     *  @param[in] addr - MCTP address of terminus
     *  @param[in] terminiNames - MCTP terminus name
     */
    virtual std::optional<mctp_eid_t> getActiveEidByName(
        const std::string& terminusName) = 0;

    virtual void handleConfigurations(const Configurations& /*configurations*/)
    {}
    virtual ~MctpDiscoveryHandlerIntf() {}
};

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
     *  @param[in] event - reference to the main event loop, used for
     *                     bridgeLearnTimer's bridge-pool-learn retries
     */
    explicit MctpDiscovery(
        sdbusplus::bus_t& bus,
        std::initializer_list<MctpDiscoveryHandlerIntf*> list,
        sdeventplus::Event& event);

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

    /** @brief The existing MCTP endpoints */
    MctpInfos existingMctpInfos;

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

    /** @brief Helper function to invoke registered handlers for
     *  the added MCTP endpoints
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos);

    /** @brief Helper function to invoke registered handlers for
     *  the removed MCTP endpoints
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
     *  @param[in] mctpInfoMap - information of discovered MCTP endpoints
     *  and the availability status of each endpoint
     */
    void getMctpInfos(std::map<MctpInfo, Availability>& mctpInfoMap);

    /** @brief Get list of new MctpInfos in addedInterace D-Bus signal message.
     *
     *  @param[in] msg - addedInterace D-Bus signal message
     *  @param[in] mctpInfos - information of added MCTP endpoints
     */
    void getAddedMctpInfos(sdbusplus::message_t& msg, MctpInfos& mctpInfos);

    /** @brief Add new MctpInfos to existingMctpInfos.
     *
     *  @param[in] mctpInfos - information of new MCTP endpoints
     */
    void addToExistingMctpInfos(const MctpInfos& mctpInfos);

    /** @brief Erase the removed MCTP endpoint from existingMctpInfos.
     *
     *  @param[in] mctpInfos - the remaining MCTP endpoints
     *  @param[out] removedInfos - the removed MCTP endpoints
     */
    void removeFromExistingMctpInfos(MctpInfos& mctpInfos,
                                     MctpInfos& removedInfos);

    friend class ::TestMctpDiscovery;

  private:
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

    /** @brief Map to store D-Bus match objects for deferred association
     * discovery */
    std::map<std::string, std::unique_ptr<sdbusplus::bus::match_t>>
        associationMatches;

    /** @brief Cache of resolved bridge pool ranges, keyed by the
     * bridge's own EID. */
    std::map<eid, std::pair<eid, eid>> bridgePoolCache;

    /** @brief Endpoints awaiting a bridge that has not resolved yet. */
    std::vector<MctpInfo> pendingBridgeChildren;

    /** @brief Bridge-pool-learn retry state for one bridge: which
     * PoolOffset values (from its EM config, never the raw pool range -
     * see getConfiguredPoolOffsets) still haven't had a successful
     * LearnEndpoint, and how many retry rounds have been spent on them.
     */
    struct PendingBridgeLearn
    {
        NetworkId networkId;
        eid poolStart;
        std::set<uint64_t> pendingOffsets;
        int attempts = 0;
    };

    /** @brief Bridges with at least one PoolOffset that failed
     * LearnEndpoint, keyed by the bridge's own EID. Entries are removed
     * once every configured offset succeeds, or after
     * bridgeLearnMaxAttempts retries (see retryBridgeLearn) - a
     * permanently-unresponsive device (e.g. a hardware fault) shouldn't
     * be retried forever.
     */
    std::map<eid, PendingBridgeLearn> pendingBridgeLearns;

    /** @brief How often retryBridgeLearn re-attempts pendingBridgeLearns.
     * Matches mctp-reactor's own retry cadence (MCTPReactor tick()),
     * since both are working around the same class of problem: a
     * downstream device that may still be powering on.
     */
    static constexpr auto bridgeLearnRetryInterval = std::chrono::seconds(5);

    /** @brief Give up on a bridge's still-failing offsets after this many
     * retryBridgeLearn rounds (~60s at bridgeLearnRetryInterval) rather
     * than retrying a genuinely-dead device forever.
     */
    static constexpr int bridgeLearnMaxAttempts = 12;

    /** @brief Timer driving retryBridgeLearn. Only armed while
     * pendingBridgeLearns is non-empty - idle (no CPU/D-Bus cost) the
     * rest of the time, same as mctp-reactor's tick() effectively is
     * once every device it tracks is assigned.
     */
    sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic>
        bridgeLearnTimer;

    /** @brief Resolve the association for the MCTP endpoint and update the
     * internal configuration map if the association is found.
     */
    bool resolveAssociation(const pldm::utils::DBusHandler& handler,
                            MctpInfo& mctpInfo);

    /** @brief Fallback for downstream endpoints bridged behind a
     * mctpd-managed bridge with no configured_by association of their
     * own: identify which known bridge's
     * mctpd-allocated EID pool the endpoint's EID falls into, then match
     * a MCTPBridgeChild record sharing that bridge's parent inventory
     * path by PoolOffset (eid - PoolStart).
     */
    bool matchBridgePoolConfig(const pldm::utils::DBusHandler& handler,
                               MctpInfo& mctpInfo);

    /** @brief Query mctpd's Bridge1 D-Bus interface for a terminus's
     * downstream EID pool range, caching the result. Returns
     * std::nullopt if the terminus has no Bridge1 interface (i.e. is not
     * a bridge).
     */
    std::optional<std::pair<eid, eid>> getBridgePoolRange(
        const pldm::utils::DBusHandler& handler, NetworkId networkId,
        eid bridgeEid);

    /** @brief Retry bridge-pool matching for endpoints that could not be
     * matched because their bridge was not yet known; called whenever a
     * new configuration is resolved, in case it turns out to be a
     * bridge.
     */
    void retryPendingBridgeChildren(const pldm::utils::DBusHandler& handler,
                                    eid bridgeEid);

    /** @brief Ask mctpd to learn every EID in a bridge's downstream
     * pool. mctp-reactor only sets up the bridge's own endpoint (which
     * allocates the pool); nothing else enumerates the pool's members,
     * so this closes that gap directly from pldmd once a bridge is
     * confirmed. LearnEndpoint is idempotent in mctpd (a no-op for an
     * already-known EID), so this can be called freely without
     * tracking which members are already known.
     */
    void learnBridgePoolMembers(const pldm::utils::DBusHandler& handler,
                                NetworkId networkId, eid bridgeEid,
                                const std::string& bridgeObjPath, eid poolStart,
                                eid poolEnd);

    /** @brief Query which PoolOffset values actually have a
     * MCTPBridgeChild EntityManager record under a bridge's parent
     * board path, so learnBridgePoolMembers()/retryBridgeLearn() only
     * ever attempt LearnEndpoint for EIDs that are supposed to have a
     * device - never permanently-unwired pool offsets. Same
     * getSubtree query matchBridgePoolConfig() uses, scoped to the
     * bridge's parent path.
     */
    std::set<uint64_t> getConfiguredPoolOffsets(
        const pldm::utils::DBusHandler& handler,
        const std::string& bridgeObjPath);

    /** @brief bridgeLearnTimer callback - retry LearnEndpoint for every
     * still-pending offset in pendingBridgeLearns. A downstream device
     * may still be powering on when a bridge is first confirmed, so an
     * initial LearnEndpoint failure isn't necessarily permanent.
     */
    void retryBridgeLearn();
};

} // namespace pldm
