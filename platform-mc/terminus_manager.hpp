#pragma once

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "terminus.hpp"

namespace pldm
{
namespace platform_mc
{
class Manager;
constexpr size_t tidPoolSize = 255;

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
        Manager* manager) :
        event(event),
        handler(handler), requester(requester), termini(termini),
        tidPool(tidPoolSize, 0), manager(manager)
    {}

    void discoverTerminus(const MctpInfos& mctpInfos)
    {
        discoverTerminusTask(mctpInfos);
    }

    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen);
    std::optional<uint8_t> toEID(uint8_t tid);
    std::optional<uint8_t> toTID(uint8_t eid);
    std::optional<uint8_t> mapToTID(uint8_t eid);
    void unmapTID(uint8_t tid);

  private:
    requester::Coroutine initTerminus(const MctpInfo& mctpInfo);
    int sendPldmMsg(mctp_eid_t eid, std::shared_ptr<Request> requestMsg,
                    std::shared_ptr<Response> responseMsg);
    requester::Coroutine discoverTerminusTask(const MctpInfos& mctpInfos);
    requester::Coroutine getTID(mctp_eid_t eid, uint8_t& tid);
    requester::Coroutine getPLDMTypes(mctp_eid_t eid, uint64_t& supportedTypes);
    requester::Coroutine setTID(mctp_eid_t eid, uint8_t tid);
    requester::Coroutine getPDRs(std::shared_ptr<Terminus> terminus);
    requester::Coroutine getPDR(mctp_eid_t eid, uint32_t recordHndl,
                                uint32_t dataTransferHndl,
                                uint8_t transferOpFlag, uint16_t requestCnt,
                                uint16_t recordChgNum, uint32_t& nextRecordHndl,
                                uint32_t& nextDataTransferHndl,
                                uint8_t& transferFlag, uint16_t& responseCnt,
                                std::vector<uint8_t>& recordData,
                                uint8_t& transferCrc);

    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    dbus_api::Requester& requester;
    std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini;
    std::vector<uint8_t> tidPool;
    Manager* manager;
};
} // namespace platform_mc
} // namespace pldm
