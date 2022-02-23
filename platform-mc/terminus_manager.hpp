#pragma once

#include "config.h"

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensor_manager.hpp"
#include "terminus.hpp"

#include <queue>

namespace pldm
{
namespace platform_mc
{
constexpr size_t tidPoolSize = 255;
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
        std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini,
        SensorManager& sensorManager) :
        event(event),
        handler(handler), requester(requester), termini(termini),
        tidPool(tidPoolSize, 0), sensorManager(sensorManager)
    {}

    /** @brief start a coroutine to discover terminus
     *
     *  @param[in] mctpInfos - list of EID/UUID to be checked
     */
    void discoverTerminus(const MctpInfos& mctpInfos)
    {
        if (queuedMctpInfos.empty())
        {
            queuedMctpInfos.emplace(mctpInfos);
            if (discoverTerminusTaskHandle && discoverTerminusTaskHandle.done())
            {
                discoverTerminusTaskHandle.destroy();
            }
            auto co = discoverTerminusTask();
            discoverTerminusTaskHandle = co.handle;
        }
        else
        {
            queuedMctpInfos.emplace(mctpInfos);
        }
    }

    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen);

    /** @brief member functions to map/unmap TID to/from EID
     */
    std::optional<mctp_eid_t> toEID(PldmTID tid);
    std::optional<PldmTID> toTID(mctp_eid_t eid);
    std::optional<PldmTID> mapToTID(mctp_eid_t eid);
    void unmapTID(PldmTID tid);

  private:
    /** @brief Initialize terminus and then instantiate terminus object to keeps
     *         the date fetched from terminus
     *
     *  @param[in] mctpInfo - EID and UUID
     */
    requester::Coroutine initTerminus(const MctpInfo& mctpInfo);

    /** @brief Send request PLDM message to destination EID. The function will
     *         return when received the response message from terminus.
     *
     *  @param[in] eid - destination EID
     *  @param[in] requestMsg - request PLDM message
     *  @param[out] responseMsg - response PLDM message
     *  @return coroutine return_value - PLDM completion code
     */
    int sendPldmMsg(mctp_eid_t eid, std::shared_ptr<Request> requestMsg,
                    std::shared_ptr<Response> responseMsg);

    /** @brief The coroutine task execute by discoverTerminus()
     *
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine discoverTerminusTask();

    /** @brief Send getTID PLDM command to destination EID and then return the
     *         value of tid in reference parameter.
     *
     *  @param[in] eid - Destination EID
     *  @param[out] tid - TID returned from terminus
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getTID(mctp_eid_t eid, PldmTID& tid);

    /** @brief Send getPLDMTypes command to destination EID and then return the
     *         value of supportedTypes in reference parameter.
     *
     *  @param[in] eid - Destination EID
     *  @param[out] supportedTypes - Supported Types returned from terminus
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getPLDMTypes(mctp_eid_t eid, uint64_t& supportedTypes);

    /** @brief Send setTID command to destination EID.
     *
     *  @param[in] eid - Destination EID
     *  @param[in] tid - Terminus ID
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine setTID(mctp_eid_t eid, PldmTID tid);

    /** @brief Fetch all PDRs from terminus.
     *
     *  @param[in] terminus - The terminus object to store fetched PDRs
     *  @return coroutine return_value - PLDM completion code
     */
    requester::Coroutine getPDRs(std::shared_ptr<Terminus> terminus);

    /** @brief Fetch PDR from terminus
     *
     *  @param[in] eid - Destination EID
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
    requester::Coroutine getPDR(mctp_eid_t eid, uint32_t recordHndl,
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
    std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini;

    /** @brief A table for maintaining assigned TID */
    std::vector<mctp_eid_t> tidPool;

    /** @brief A reference to SensorManager **/
    SensorManager& sensorManager;

    /** @brief A queue of MctpInfos to be discovered **/
    std::queue<MctpInfos> queuedMctpInfos{};

    /** @brief coroutine handle of discoverTerminusTask */
    std::coroutine_handle<> discoverTerminusTaskHandle;
};
} // namespace platform_mc
} // namespace pldm
