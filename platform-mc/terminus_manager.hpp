#pragma once

#include "config.h"

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

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

/**
 * @brief TerminusManager
 *
 * TerminusManager class manages PLDM Platform Monitoring and Control devices.
 * It discovers and initializes the PLDM terminus.
 */
class TerminusManager
{
  public:
    TerminusManager() = delete;
    TerminusManager(const TerminusManager&) = delete;
    TerminusManager(TerminusManager&&) = delete;
    TerminusManager& operator=(const TerminusManager&) = delete;
    TerminusManager& operator=(TerminusManager&&) = delete;
    ~TerminusManager() = default;

    explicit TerminusManager(
        sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        dbus_api::Requester& requester,
        std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini) :
        event(event),
        handler(handler), requester(requester), termini(termini),
        tidPool(tidPoolSize, false)
    {
        // DSP0240 v1.1.0 table-8, special value: 0,0xFF = reserved
        tidPool[0] = true;
        tidPool[PLDM_TID_RESERVED] = true;
    }

    /** @brief start a coroutine to discover terminus
     *
     *  @param[in] mctpInfos - list of EID/UUID to be checked
     */
    void discoverMctpTerminus(const MctpInfos& mctpInfos)
    {
        if (queuedMctpInfos.empty())
        {
            queuedMctpInfos.emplace(mctpInfos);
            if (discoverTerminusTaskHandle && discoverTerminusTaskHandle.done())
            {
                discoverTerminusTaskHandle.destroy();
            }
            auto co = discoverMctpTerminusTask();
            discoverTerminusTaskHandle = co.handle;
        }
        else
        {
            queuedMctpInfos.emplace(mctpInfos);
        }
    }

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

    /** @brief member functions to map/unmap tid
     */
    std::optional<MctpInfo> toMctpInfo(const tid_t& tid);
    std::optional<tid_t> toTid(const MctpInfo& mctpInfo);
    std::optional<tid_t> mapTid(const MctpInfo& mctpInfo);
    std::optional<tid_t> mapTid(const MctpInfo& mctpInfo, tid_t tid);
    void unmapTid(const tid_t& tid);

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
    requester::Coroutine getTidOverMctp(mctp_eid_t eid, tid_t& tid);

    /** @brief Send setTID command to destination EID.
     *
     *  @param[in] eid - Destination EID
     *  @param[in] tid - Terminus ID
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine setTidOverMctp(mctp_eid_t eid, tid_t tid);

    /** @brief Send getPLDMTypes command to destination EID and then return the
     *         value of supportedTypes in reference parameter.
     *
     *  @param[in] tid - Destination TID
     *  @param[out] supportedTypes - Supported Types returned from terminus
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getPLDMTypes(tid_t tid, uint64_t& supportedTypes);

    /** @brief Fetch all PDRs from terminus.
     *
     *  @param[in] terminus - The terminus object to store fetched PDRs
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getPDRs(std::shared_ptr<Terminus> terminus);

    /** @brief Fetch PDR from terminus
     *
     *  @param[in] tid - Destination TID
     *  @param[in] recordHndl - Record handle
     *  @param[in] dataTransferHndl - Data transfer handle
     *  @param[in] transferOpFlag - Transfer Operation Flag
     *  @param[in] requstCnt - Request Count of data
     *  @param[in] recordChgNum - Record change number
     *  @param[out] nextRecordHndl - Next record handle
     *  @param[out] nextDataTransferHndl - Next data transfer handle
     *  @param[out] transferFlag - Transfer flag
     *  @param[out] responseCnt - Response count of record data
     *  @param[out] recordData - Returned record data
     *  @param[out] transferCrc - CRC value when record data is last part of PDR
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getPDR(tid_t tid, uint32_t recordHndl,
                                uint32_t dataTransferHndl,
                                uint8_t transferOpFlag, uint16_t requestCnt,
                                uint16_t recordChgNum, uint32_t& nextRecordHndl,
                                uint32_t& nextDataTransferHndl,
                                uint8_t& transferFlag, uint16_t& responseCnt,
                                std::vector<uint8_t>& recordData,
                                uint8_t& transferCrc);

    sdeventplus::Event& event;
    RequesterHandler& handler;
    dbus_api::Requester& requester;

    /** @brief Managed termini list */
    std::map<tid_t, std::shared_ptr<Terminus>>& termini;

    /** @brief tables for maintaining assigned TID */
    std::vector<bool> tidPool;
    std::map<tid_t, SupportedTransportLayer> transportLayerTable;
    std::map<tid_t, MctpInfo> mctpInfoTable;

    /** @brief A queue of MctpInfos to be discovered **/
    std::queue<MctpInfos> queuedMctpInfos{};

    /** @brief coroutine handle of discoverTerminusTask */
    std::coroutine_handle<> discoverTerminusTaskHandle;
};
} // namespace platform_mc
} // namespace pldm
