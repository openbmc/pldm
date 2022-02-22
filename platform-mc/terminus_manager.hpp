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

    explicit TerminusManager(sdeventplus::Event& event,
                             requester::Handler<requester::Request>& handler,
                             dbus_api::Requester& requester,
                             std::vector<std::shared_ptr<Terminus>>& terminuses,
                             Manager* manager) :
        event(event),
        handler(handler), requester(requester), terminuses(terminuses),
        tidPool(255, 0), manager(manager)
    {}

    void discoverTerminus(const MctpInfos& mctpInfos);
    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen);

    void sendGetTID(mctp_eid_t eid);
    void handleRespGetTID(mctp_eid_t eid, const pldm_msg* response,
                          size_t respMsgLen);
    void sendSetTID(mctp_eid_t eid, uint8_t tid);
    void handleRespSetTID(mctp_eid_t eid, const pldm_msg* response,
                          size_t respMsgLen);

    std::optional<uint8_t> getMappedEID(uint8_t tid);
    std::optional<uint8_t> getMappedTID(uint8_t eid);
    std::optional<uint8_t> mapTID(uint8_t eid);
    void unmapTID(uint8_t tid);
    void scanTerminus();
    void initTerminus();

  private:
    void processScanTerminusEvent(sdeventplus::source::EventBase& source);
    void processInitTerminusEvent(sdeventplus::source::EventBase& source);

    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    dbus_api::Requester& requester;
    std::vector<std::shared_ptr<Terminus>>& terminuses;

    std::vector<uint8_t> tidPool;
    std::unique_ptr<sdeventplus::source::Defer> deferredScanTerminusEvent;
    MctpInfos _mctpInfos;
    std::unique_ptr<sdeventplus::source::Defer> deferredInitTerminusEvent;
    std::vector<std::shared_ptr<Terminus>> _terminuses;

    Manager* manager;
};
} // namespace platform_mc
} // namespace pldm