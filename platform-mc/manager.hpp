#pragma once

#include "libpldm/pldm.h"

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "platform_manager.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{
using namespace pldm::pdr;

/**
 * @brief Manager
 *
 * This class handles all the aspect of the PLDM Platform Monitoring and Control
 * specification for the MCTP devices
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
                     pldm::InstanceIdDb& instanceIdDb) :
        terminusManager(event, handler, instanceIdDb, termini, this),
        platformManager(terminusManager, termini)
    {}

    /** @brief Helper function to do the actions before discovery terminus
     */
    exec::task<int> beforeDiscoverTerminus();

    /** @brief Helper function to do the actions after discovery terminu
     */
    exec::task<int> afterDiscoverTerminus();

    /** @brief Helper function to invoke registered handlers for
     *  the added MCTP endpoints
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos)
    {
        terminusManager.discoverMctpTerminus(mctpInfos);
    }

    /** @brief Helper function to invoke registered handlers for
     *  the removed MCTP endpoints
     *
     *  @param[in] mctpInfos - information of removed MCTP endpoints
     */
    void handleRemovedMctpEndpoints(const MctpInfos& mctpInfos)
    {
        terminusManager.removeMctpTerminus(mctpInfos);
    }

  private:
    /** @brief List of discovered termini */
    std::map<pldm_tid_t, std::shared_ptr<Terminus>> termini{};

    /** @brief Terminus interface for calling the hook functions */
    TerminusManager terminusManager;

    /** @brief Platform interface for calling the hook functions */
    PlatformManager platformManager;
};
} // namespace platform_mc
} // namespace pldm
