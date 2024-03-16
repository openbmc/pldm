#pragma once

#include "config.h"

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "terminus.hpp"

#include <queue>

namespace pldm
{

enum SupportedTransportLayer
{
    MCTP
};

namespace platform_mc
{

/** @brief Sise of TID Pool in pldmd */
constexpr size_t tidPoolSize = std::numeric_limits<pldm_tid_t>::max() + 1;
using RequesterHandler = requester::Handler<requester::Request>;

class Manager;
/**
 * @brief TerminusManager
 *
 * TerminusManager class to discover and initialize PLDM terminus.
 */
class TerminusManager
{
  public:
    TerminusManager() = delete;
    TerminusManager(const TerminusManager&) = delete;
    TerminusManager(TerminusManager&&) = delete;
    TerminusManager& operator=(const TerminusManager&) = delete;
    TerminusManager& operator=(TerminusManager&&) = delete;
    virtual ~TerminusManager() = default;

    explicit TerminusManager(
        sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        pldm::InstanceIdDb& instanceIdDb,
        std::map<pldm_tid_t, std::shared_ptr<Terminus>>& termini,
        mctp_eid_t localEid, Manager* manager) :
        event(event),
        handler(handler), instanceIdDb(instanceIdDb), termini(termini),
        localEid(localEid), tidPool(tidPoolSize, false), manager(manager)
    {
        // DSP0240 v1.1.0 table-8, special value: 0,0xFF = reserved
        tidPool[0] = true;
        tidPool[PLDM_TID_RESERVED] = true;
    }

    /** @brief start a coroutine to discover terminus
     *
     *  @param[in] mctpInfos - list of mctpInfo to be checked
     */
    void discoverMctpTerminus(const MctpInfos& mctpInfos);

    /** @brief remove MCTP endpoints
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    void removeMctpTerminus(const MctpInfos& mctpInfos);

    /** @brief Send request PLDM message to tid. The function will return when
     *         received the response message from terminus. The function will
     *         auto get the instanceID from libmctp and update to request
     *         message.
     *
     *  @param[in] tid - terminus tid
     *  @param[in] request - request PLDM message
     *  @param[out] responseMsg - response PLDM message
     *  @param[out] responseLen - length of response PLDM message
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> SendRecvPldmMsg(pldm_tid_t tid, Request& request,
                                    const pldm_msg** responseMsg,
                                    size_t* responseLen);

    /** @brief Send request PLDM message to eid. The function will
     *         return when received the response message from terminus.
     *
     *  @param[in] eid - eid
     *  @param[in] request - request PLDM message
     *  @param[out] responseMsg - response PLDM message
     *  @param[out] responseLen - length of response PLDM message
     *  @return coroutine return_value - PLDM completion code
     */
    virtual exec::task<int>
        SendRecvPldmMsgOverMctp(mctp_eid_t eid, Request& request,
                                const pldm_msg** responseMsg,
                                size_t* responseLen);

    /** @brief member functions to map/unmap tid
     */
    std::optional<MctpInfos> toMctpInfos(const pldm_tid_t& tid);

    /** @brief Member functions to response the TID of specific MCTP interface
     *
     *  @param[in] mctpInfos - information of MCTP endpoints
     *
     *  @return tid - Terminus tid
     */
    std::optional<pldm_tid_t> toTid(const MctpInfo& mctpInfo);

    /** @brief Member functions to find the TID for MCTP interface. Response the
     *         Terminus TID when mctpInfo is already in the data base. Response
     *         new tid from pool when mctpInfo is new.
     *
     *  @param[in] mctpInfos - information of MCTP endpoints
     *
     *  @return tid - Terminus tid
     */
    std::optional<pldm_tid_t> mapTid(const MctpInfo& mctpInfo);

    /** @brief Member functions to store the mctp info and tid to terminus info
     *         list.
     *
     *  @param[in] mctpInfos - information of MCTP endpoints
     *  @param[in] tid - Terminus tid
     *
     *  @return tid - Terminus tid
     */
    std::optional<pldm_tid_t> storeTerminusInfo(const MctpInfo& mctpInfo,
                                                pldm_tid_t tid);

    /** @brief Member functions to remove the TID from the transportLayer and
     *         mctpInfo table
     *
     *  @param[in] tid - Terminus tid
     *
     *  @return true/false - True when tid in the table otherwise return false
     */
    bool unmapTid(const pldm_tid_t& tid);

    /** @brief Member functions to remove the mctpInfo from the transportLayer
     *         and mctpInfo table of one TID
     *
     *  @param[in] tid - Terminus tid
     *
     *  @return true/false - True when tid in the table otherwise return false
     */
    bool unmapMctpInfo(const pldm_tid_t& tid, const MctpInfo& mctpInfo);

    /** @brief getter of local EID
     *
     *  @return uint8_t - local EID
     */
    mctp_eid_t getLocalEid()
    {
        return localEid;
    }

    /** @brief Find the terminus object pointer in termini list.
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    std::map<pldm_tid_t, std::shared_ptr<Terminus>>::iterator
        findTeminusPtr(const MctpInfo& mctpInfo);

  private:
    /** @brief The coroutine task execute by discoverMctpTerminus()
     *
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> discoverMctpTerminusTask();

    /** @brief Initialize terminus and then instantiate terminus object to keeps
     *         the data fetched from terminus
     *
     *  @param[in] mctpInfo - NetworkId, EID and UUID
     */
    exec::task<int> initMctpTerminus(const MctpInfo& mctpInfo);

    /** @brief Send getTID PLDM command to destination EID and then return the
     *         value of tid in reference parameter.
     *
     *  @param[in] eid - Destination EID
     *  @param[out] tid - TID returned from terminus
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> getTidOverMctp(mctp_eid_t eid, pldm_tid_t* tid);

    /** @brief Send setTID command to destination EID.
     *
     *  @param[in] eid - Destination EID
     *  @param[in] tid - Terminus ID
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> setTidOverMctp(mctp_eid_t eid, pldm_tid_t tid);

    /** @brief Send getPLDMTypes command to destination TID and then return the
     *         value of supportedTypes in reference parameter.
     *
     *  @param[in] tid - Destination TID
     *  @param[out] supportedTypes - Supported Types returned from terminus
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> getPLDMTypes(pldm_tid_t tid, uint64_t& supportedTypes);

    /** @brief Reference to to PLDM daemon's main event loop.
     */
    sdeventplus::Event& event;

    /** @brief Reference to a Handler object that manages the request/response
     *         logic.
     */
    RequesterHandler& handler;

    /** @brief Reference to the instanceID data base from libpldm */
    pldm::InstanceIdDb& instanceIdDb;

    /** @brief Managed termini list */
    std::map<pldm_tid_t, std::shared_ptr<Terminus>>& termini;

    /** @brief local EID */
    mctp_eid_t localEid;

    /** @brief tables for maintaining assigned TID */
    std::vector<bool> tidPool;

    /** @brief Store the supported transport layers of specific TID */
    std::map<pldm_tid_t, SupportedTransportLayer> transportLayerTable;

    /** @brief Store the supported MCTP interface info of specific TID */
    std::map<pldm_tid_t, MctpInfos> mctpInfoTable;

    /** @brief A queue of MctpInfos to be discovered **/
    std::queue<MctpInfos> queuedMctpInfos{};

    /** @brief coroutine handle of discoverTerminusTask */
    std::optional<std::pair<exec::async_scope, std::optional<int>>>
        discoverMctpTerminusTaskHandle{};

    /** @brief A Manager interface for calling the hook functions **/
    Manager* manager;
};
} // namespace platform_mc
} // namespace pldm
