#pragma once

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "common/types.hpp"
#include "common/utils.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "terminus.hpp"
#include "terminus_manager.hpp"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <vector>

namespace pldm
{
namespace platform_mc
{

using HandlerFunc =
    std::function<int(uint8_t, uint8_t, uint16_t, const std::vector<uint8_t>)>;

/**
 * @brief EventManager
 *
 * This class manages the event from/to terminus and provides
 * function calls for other classes to start/stop event/error monitoring.
 *
 */
class EventManager
{
  public:
    EventManager() = delete;
    EventManager(const EventManager&) = delete;
    EventManager(EventManager&&) = delete;
    EventManager& operator=(const EventManager&) = delete;
    EventManager& operator=(EventManager&&) = delete;
    virtual ~EventManager() = default;

    explicit EventManager(sdeventplus::Event& event,
                          TerminusManager& terminusManager,
                          std::map<tid_t, std::shared_ptr<Terminus>>& termini) :
        event(event),
        terminusManager(terminusManager), termini(termini),
        pollingTime(PLATFORM_EVENT_MSG_POLLING_TIME),
        pollingCriticalEventTime(PLATFORM_EVENT_MSG_POLLING_CRITICAL_EVENT_TIME)
    {
        pollReqTimeoutTimer = std::make_unique<phosphor::Timer>(
            [&](void) { pollReqTimeoutHdl(); });
        handlePldmDbusEventSignal();
        /* Init reqData data*/
        memset(&reqData, 0, sizeof(struct ReqPollInfo));
    }

    /** @brief starting events/errors polling task
     */
    void startPolling();

    /** @brief stopping events/errors polling task
     */
    void stopPolling();

    /** @brief Register the callback to handle event class
     */
    void registerEventHandler(uint8_t event_class, HandlerFunc func);

    /** @brief Adds eventID to critical event polling queue
     *  @param[in] tid: Terminus tid
     *  @param[in] eventId: Event id
     *  @return None
     */
    virtual int enqueueCriticalEvent(uint8_t tid, uint16_t eventId);

    /** @brief Handle property-changed signals in PldmMessagePollEvent D-Bus
     * interface. Adds the eventID to corresponding terminus queue base on
     * the TID in the D-Bus message. The terminus will send the requests to get
     * the message data.
     *  @param[in] None
     *  @return None
     */
    virtual void handlePldmDbusEventSignal();

    /** @brief Feed Event to Polling loop
     */
    virtual int feedCriticalEventCb();

    /** @brief start a coroutine for polling all events/errors.
     */
    virtual void doPolling()
    {
        if (doPollingTaskHandle)
        {
            if (doPollingTaskHandle.done())
            {
                doPollingTaskHandle.destroy();
                auto co = doPollingTask();
                doPollingTaskHandle = co.handle;
                if (doPollingTaskHandle.done())
                {
                    doPollingTaskHandle = nullptr;
                }
            }
        }
        else
        {
            auto co = doPollingTask();
            doPollingTaskHandle = co.handle;
            if (doPollingTaskHandle.done())
            {
                doPollingTaskHandle = nullptr;
            }
        }
    }

  protected:
    std::size_t MAX_QUEUE_SIZE = 256;

    enum queue_errors
    {
        QUEUE_ADD_SUCCESS = 0,
        QUEUE_OVER_FLOW = -1,
        QUEUE_ITEM_EXIST = -2,
    };

    struct ReqPollInfo
    {
        uint8_t tid;
        uint8_t operationFlag;
        uint32_t dataTransferHandle;
        uint16_t eventIdToAck;
    };

    struct RecvPollInfo
    {
        uint8_t eventClass;
        uint32_t totalSize;
        std::vector<uint8_t> data;
    };

    struct EventInfo
    {
        uint8_t tid;
        uint32_t eventId;
    };

    bool isProcessPolling = false;
    bool responseReceived = false;
    bool isPolling = false;
    bool isCritical = false;

    /** @brief polling all events in each terminus
     */
    requester::Coroutine doPollingTask();

    void reset();
    void pollReqTimeoutHdl();
    void pollEventReqCb();

    void processResponseMsg(mctp_eid_t eid, const pldm_msg* response,
                            size_t respMsgLen);

    sdeventplus::Event& event;

    /** @brief reference of terminusManager */
    TerminusManager& terminusManager;

    /** @brief List of discovered termini */
    std::map<tid_t, std::shared_ptr<Terminus>>& termini;

    /** @brief poll for platform event message interval in miliseconds. */
    uint32_t pollingTime;

    /** @brief poll for critical event message interval in miliseconds. */
    uint32_t pollingCriticalEventTime;

    /** @brief sensor polling timer */
    std::unique_ptr<phosphor::Timer> pollReqTimeoutTimer;

    /** @brief sensor polling timer */
    std::unique_ptr<phosphor::Timer> pollTimer;

    /** @brief sensor polling timer */
    std::unique_ptr<phosphor::Timer> pollCriticalEventTimer;

    /** @brief Handle the response data of one eventID*/
    std::map<uint8_t, std::vector<HandlerFunc>> eventHndls;

    /** @brief coroutine handle of doSensorPollingTask */
    std::coroutine_handle<> doPollingTaskHandle;

    /** @brief critical eventID queue */
    std::deque<std::pair<uint8_t, uint16_t>> critEventQueue;

    /** @brief request Data in PollForPlatformEventMessage */
    ReqPollInfo reqData;

    /** @brief response Data from PollForPlatformEventMessage */
    RecvPollInfo recvData;

    std::unique_ptr<sdbusplus::bus::match_t> pldmEventSignal;

  private:
    void doFeedCriticalEventCb();
};
} // namespace platform_mc
} // namespace pldm
