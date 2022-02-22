#pragma once

#include "libpldm/requester/pldm.h"

#include "common/types.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "terminus_manager.hpp"

#include <vector>

namespace pldm
{
namespace platform_mc
{
using namespace pldm::dbus_api;
using namespace pldm::pdr;

/**
 * @brief Manager
 *
 * This class handles all the aspect of the PLDM Platform Monitoring and Control
 * sepcification for the MCTP devices
 */
class Manager : public pldm::MctpDiscoveryHandlerIntf
{
  public:
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager& operator=(Manager&&) = delete;
    ~Manager() = default;

    explicit Manager(sdeventplus::Event& event,
                     requester::Handler<requester::Request>& handler,
                     Requester& requester) :
        terminusManager(event, handler, requester, termini, this)
    {}

    void handleMCTPEndpoints(const MctpInfos& mctpInfos)
    {
        terminusManager.discoverTerminus(mctpInfos);
    }

    Response handleRequest(mctp_eid_t eid, Command command,
                           const pldm_msg* request, size_t reqMsgLen)
    {
        return terminusManager.handleRequest(eid, command, request, reqMsgLen);
    }

  private:
    /** @brief Array of discovered Termunuses */
    std::map<mctp_eid_t, std::shared_ptr<Terminus>> termini{};

    TerminusManager terminusManager;
};
} // namespace platform_mc
} // namespace pldm
