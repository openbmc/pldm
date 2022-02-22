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
constexpr size_t tidPoolSize = std::numeric_limits<tid_t>::max() + 1;
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
        std::map<tid_t, std::shared_ptr<Terminus>>& termini, Manager* manager) :
        event(event),
        handler(handler), instanceIdDb(instanceIdDb), termini(termini),
        tidPool(tidPoolSize, false), manager(manager)
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

    /** @brief Send request PLDM message to tid. The function will
     *         return when received the response message from terminus.
     *
     *  @param[in] tid - tid
     *  @param[in] request - request PLDM message
     *  @param[out] responseMsg - response PLDM message
     *  @param[out] responseLen - length of response PLDM message
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine SendRecvPldmMsg(tid_t tid, Request& request,
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
    virtual requester::Coroutine
        SendRecvPldmMsgOverMctp(mctp_eid_t eid, Request& request,
                                const pldm_msg** responseMsg,
                                size_t* responseLen);

    /** @brief member functions to map/unmap tid
     */
    std::optional<MctpInfo> toMctpInfo(const tid_t& tid);
    std::optional<tid_t> toTid(const MctpInfo& mctpInfo);
    std::optional<tid_t> mapTid(const MctpInfo& mctpInfo);
    std::optional<tid_t> mapTid(const MctpInfo& mctpInfo, tid_t tid);
    bool unmapTid(const tid_t& tid);

  private:
    /** @brief The coroutine task execute by discoverMctpTerminus()
     *
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine discoverMctpTerminusTask();

    /** @brief Initialize terminus and then instantiate terminus object to keeps
     *         the data fetched from terminus
     *
     *  @param[in] mctpInfo - NetworkId, EID and UUID
     */
    requester::Coroutine initMctpTerminus(const MctpInfo& mctpInfo);

    /** @brief Send getTID PLDM command to destination EID and then return the
     *         value of tid in reference parameter.
     *
     *  @param[in] eid - Destination EID
     *  @param[out] tid - TID returned from terminus
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getTidOverMctp(mctp_eid_t eid, tid_t* tid);

    /** @brief Find the terminus object pointer in termini list.
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    std::map<tid_t, std::shared_ptr<Terminus>>::iterator
        findTeminusPtr(const MctpInfo& mctpInfo);

    /** @brief Send setTID command to destination EID.
     *
     *  @param[in] eid - Destination EID
     *  @param[in] tid - Terminus ID
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine setTidOverMctp(mctp_eid_t eid, tid_t tid);

    /** @brief Send getPLDMTypes command to destination TID and then return the
     *         value of supportedTypes in reference parameter.
     *
     *  @param[in] tid - Destination TID
     *  @param[out] supportedTypes - Supported Types returned from terminus
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getPLDMTypes(tid_t tid, uint64_t& supportedTypes);

    sdeventplus::Event& event;
    RequesterHandler& handler;
    pldm::InstanceIdDb& instanceIdDb;

    /** @brief Managed termini list */
    std::map<tid_t, std::shared_ptr<Terminus>>& termini;

    /** @brief tables for maintaining assigned TID */
    std::vector<bool> tidPool;
    std::map<tid_t, SupportedTransportLayer> transportLayerTable;
    std::map<tid_t, MctpInfo> mctpInfoTable;

    /** @brief A queue of MctpInfos to be discovered **/
    std::queue<MctpInfos> queuedMctpInfos{};

    /** @brief coroutine handle of discoverTerminusTask */
    std::coroutine_handle<> discoverMctpTerminusTaskHandle;

    /** @brief A Manager interface for calling the hook functions **/
    Manager* manager;
};
} // namespace platform_mc
} // namespace pldm
