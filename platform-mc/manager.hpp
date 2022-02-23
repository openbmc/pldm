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
        platformManager(terminusManager, termini),
        sensorManager(event, terminusManager, termini)
    {}

    exec::task<int> beforeDiscoverTerminus();

    exec::task<int> afterDiscoverTerminus();

    void handleMctpEndpoints(const MctpInfos& mctpInfos)
    {
        terminusManager.discoverMctpTerminus(mctpInfos);
    }

    void handleRemovedMctpEndpoints(const MctpInfos& mctpInfos)
    {
        terminusManager.removeMctpTerminus(mctpInfos);
    }

    void startSensorPolling(pldm_tid_t tid)
    {
        sensorManager.startPolling(tid);
    }

    void stopSensorPolling(pldm_tid_t tid)
    {
        sensorManager.stopPolling(tid);
    }

  private:
    /** @brief List of discovered termini */
    std::map<pldm_tid_t, std::shared_ptr<Terminus>> termini{};

    TerminusManager terminusManager;
    PlatformManager platformManager;
    SensorManager sensorManager;
};
} // namespace platform_mc
} // namespace pldm
