#pragma once

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "common/types.hpp"
#include "numeric_sensor.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "terminus.hpp"
#include "terminus_manager.hpp"

#define PLDM_CPER_EVENT_CLASS 0xFA

namespace pldm
{
namespace platform_mc
{

const std::string SensorThresholdCriticalHighGoingHigh{
    "OpenBMC.0.2.SensorThresholdCriticalHighGoingHigh"};
const std::string SensorThresholdCriticalHighGoingLow{
    "OpenBMC.0.2.SensorThresholdCriticalHighGoingLow"};
const std::string SensorThresholdCriticalLowGoingHigh{
    "OpenBMC.0.2.SensorThresholdCriticalLowGoingHigh"};
const std::string SensorThresholdCriticalLowGoingLow{
    "OpenBMC.0.2.SensorThresholdCriticalLowGoingLow"};
const std::string SensorThresholdWarningHighGoingHigh{
    "OpenBMC.0.2.SensorThresholdWarningHighGoingHigh"};
const std::string SensorThresholdWarningHighGoingLow{
    "OpenBMC.0.2.SensorThresholdWarningHighGoingLow"};
const std::string SensorThresholdWarningLowGoingHigh{
    "OpenBMC.0.2.SensorThresholdWarningLowGoingHigh"};
const std::string SensorThresholdWarningLowGoingLow{
    "OpenBMC.0.2.SensorThresholdWarningLowGoingLow"};

/**
 * @brief EventManager
 *
 * This class manages PLDM events from terminus. The function includes providing
 * the API for process event data and using phosphor-logging API to log the
 * event.
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

    explicit EventManager(
        TerminusManager& terminusManager,
        std::map<mctp_eid_t, std::shared_ptr<Terminus>>& termini) :
        terminusManager(terminusManager),
        termini(termini){};

    /** @brief Handle platform event
     *
     *  @param[in] tid - tid where the event is from
     *  @param[in] eventId - event Id
     *  @param[in] eventClass - event class
     *  @param[in] eventData - event data
     *  @param[in] eventDataSize - size of event data
     *  @return PLDM completion code
     *
     */
    int handlePlatformEvent(pldm_tid_t tid, uint16_t eventId,
                            uint8_t eventClass, const uint8_t* eventData,
                            size_t eventDataSize);

    std::string getSensorThresholdMessageId(uint8_t previousEventState,
                                            uint8_t eventState);

    /** @brief Set available state of terminus for pldm request.
     */
    void updateAvailableState(pldm_tid_t tid, Availability state)
    {
        availableState[tid] = state;
    };

    /** @brief Get available state of terminus for pldm request.
     */
    bool getAvailableState(pldm_tid_t tid)
    {
        if (!availableState.contains(tid))
        {
            return false;
        }
        return availableState[tid];
    };

  protected:
    virtual int processCperEvent(uint16_t eventId, const uint8_t* eventData,
                                 size_t eventDataSize);

    int createCperDumpEntry(const std::string& dataType,
                            const std::string& dataPath);

    int processNumericSensorEvent(pldm_tid_t tid, uint16_t sensorId,
                                  const uint8_t* sensorData,
                                  size_t sensorDataLength);

    virtual int createSensorThresholdLogEntry(const std::string& messageID,
                                              const std::string& sensorName,
                                              const double reading,
                                              const double threshold);

    /** @brief Reference of terminusManager */
    TerminusManager& terminusManager;

    /** @brief List of discovered termini */
    TerminiMapper& termini;

    /** @brief Available state for pldm request of terminus*/
    std::map<pldm_tid_t, Availability> availableState;
};
} // namespace platform_mc
} // namespace pldm
