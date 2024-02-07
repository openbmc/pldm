#pragma once

#include "libpldm/pldm.h"

#include "common/instance_id.hpp"
#include "common/types.hpp"
#include "event_manager.hpp"
#include "platform_manager.hpp"
#include "requester/configuration_discovery_handler.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensor_manager.hpp"
#include "terminus_manager.hpp"
#ifdef OEM_META
#include <oem/meta/platform-mc/event_oem_meta.hpp>
#endif

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

    explicit Manager(
        sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        pldm::InstanceIdDb& instanceIdDb,
        pldm::ConfigurationDiscoveryHandler* configurationDiscovery) :
        terminusManager(event, handler, instanceIdDb, termini, LOCAL_EID, this),
        platformManager(terminusManager, termini),
        sensorManager(event, terminusManager, termini, this),
        eventManager(terminusManager, termini, configurationDiscovery),
        configurationDiscovery(configurationDiscovery)
    {}

    requester::Coroutine beforeDiscoverTerminus()
    {
        co_return PLDM_SUCCESS;
    }

    requester::Coroutine afterDiscoverTerminus()
    {
        auto rc = co_await platformManager.initTerminus();
        co_return rc;
    }

    void handleMctpEndpoints(const MctpInfos& mctpInfos)
    {
        terminusManager.discoverMctpTerminus(mctpInfos);
    }

    void handleRemovedMctpEndpoints(const MctpInfos& mctpInfos)
    {
        terminusManager.removeMctpTerminus(mctpInfos);
    }

    void startSensorPolling(tid_t tid)
    {
        sensorManager.startPolling(tid);
    }

    void stopSensorPolling(tid_t tid)
    {
        sensorManager.stopPolling(tid);
    }

    int handleCperEvent(const pldm_msg* request, size_t payloadLength,
                        uint8_t /* formatVersion */, uint8_t tid,
                        size_t eventDataOffset)
    {
        auto eventData = reinterpret_cast<const uint8_t*>(request->payload) +
                         eventDataOffset;
        auto eventDataSize = payloadLength - eventDataOffset;
        eventManager.handlePlatformEvent(tid, PLDM_OEM_EVENT_CLASS_0xFA,
                                         eventData, eventDataSize);
        return PLDM_SUCCESS;
    }

    int handlePldmMessagePollEvent(const pldm_msg* request,
                                   size_t payloadLength,
                                   uint8_t /* formatVersion */, uint8_t tid,
                                   size_t eventDataOffset)
    {
        auto eventData = reinterpret_cast<const uint8_t*>(request->payload) +
                         eventDataOffset;
        auto eventDataSize = payloadLength - eventDataOffset;
        eventManager.handlePlatformEvent(tid, PLDM_MESSAGE_POLL_EVENT,
                                         eventData, eventDataSize);
        return PLDM_SUCCESS;
    }

    int handleSensorEvent(const pldm_msg* request, size_t payloadLength,
                          uint8_t /* formatVersion */, uint8_t tid,
                          size_t eventDataOffset)
    {
        auto eventData = reinterpret_cast<const uint8_t*>(request->payload) +
                         eventDataOffset;
        auto eventDataSize = payloadLength - eventDataOffset;
        eventManager.handlePlatformEvent(tid, PLDM_SENSOR_EVENT, eventData,
                                         eventDataSize);
        return PLDM_SUCCESS;
    }
#ifdef OEM_META
    int handleOemMetaEvent(const pldm_msg* request, size_t payloadLength,
                           uint8_t /* formatVersion */, uint8_t tid,
                           size_t eventDataOffset)
    {
        auto eventData = reinterpret_cast<const uint8_t*>(request->payload) +
                         eventDataOffset;
        auto eventDataSize = payloadLength - eventDataOffset;
        eventManager.handlePlatformEvent(tid, PLDM_OEM_EVENT_CLASS_0xFB,
                                         eventData, eventDataSize);
        return PLDM_SUCCESS;
    }
#endif
    requester::Coroutine pollForPlatformEvent(tid_t tid)
    {
        auto it = termini.find(tid);
        if (it != termini.end())
        {
            auto& terminus = it->second;
            co_await eventManager.pollForPlatformEventTask(tid);
            terminus->pollEvent = false;
        }
        co_return PLDM_SUCCESS;
    }

  private:
    /** @brief List of discovered termini */
    std::map<tid_t, std::shared_ptr<Terminus>> termini{};

    TerminusManager terminusManager;
    PlatformManager platformManager;
    SensorManager sensorManager;
    EventManager eventManager;
    pldm::ConfigurationDiscoveryHandler* configurationDiscovery;
};
} // namespace platform_mc
} // namespace pldm
