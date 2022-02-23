#pragma once

#include "libpldm/pldm.h"

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "platform_manager.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensor_manager.hpp"
#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

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

    explicit Manager(sdeventplus::Event& event, RequesterHandler& handler,
                     pldm::InstanceIdDb& instanceIdDb) :
        terminusManager(event, handler, instanceIdDb, termini, this),
        platformManager(terminusManager, termini),
        sensorManager(event, terminusManager, termini)
    {}

    /** @brief Helper function to do the actions before discovering terminus
     *
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> beforeDiscoverTerminus();

    /** @brief Helper function to do the actions after discovering terminus
     *
     *  @return coroutine return_value - PLDM completion code
     */
    exec::task<int> afterDiscoverTerminus();

    /** @brief Helper function to invoke registered handlers for
     *         the added MCTP endpoints
     *
     *  @param[in] mctpInfos - list information of the MCTP endpoints
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos)
    {
        terminusManager.discoverMctpTerminus(mctpInfos);
    }

    /** @brief Helper function to invoke registered handlers for
     *         the removed MCTP endpoints
     *
     *  @param[in] mctpInfos - list information of the MCTP endpoints
     */
    void handleRemovedMctpEndpoints(const MctpInfos& mctpInfos)
    {
        terminusManager.removeMctpTerminus(mctpInfos);
    }

    /** @brief Helper function to start sensor polling of the terminus TID
     */
    void startSensorPolling(pldm_tid_t tid)
    {
        sensorManager.startPolling(tid);
    }

    /** @brief Helper function to stop sensor polling of the terminus TID
     */
    void stopSensorPolling(pldm_tid_t tid)
    {
        sensorManager.stopPolling(tid);
    }

  private:
    /** @brief List of discovered termini */
    TerminiMapper termini{};

    /** @brief Terminus interface for calling the hook functions */
    TerminusManager terminusManager;

    /** @brief Platform interface for calling the hook functions */
    PlatformManager platformManager;

    /** @brief Store platform manager handler */
    SensorManager sensorManager;
};
} // namespace platform_mc
} // namespace pldm
