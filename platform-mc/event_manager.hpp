#pragma once

#include "libpldm/platform.h"
#include "libpldm/pldm.h"

#include "common/types.hpp"
#include "numeric_sensor.hpp"
#include "pldmd/dbus_impl_requester.hpp"
#include "requester/handler.hpp"
#include "terminus.hpp"
#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

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

    explicit EventManager(TerminusManager& terminusManager,
                          TerminiMapper& termini) :
        terminusManager(terminusManager), termini(termini) {};

    /** @brief Handle platform event
     *
     *  @param[in] tid - tid where the event is from
     *  @param[in] eventId - event Id
     *  @param[in] eventClass - event class
     *  @param[in] eventData - event data
     *  @param[in] eventDataSize - size of event data
     *  @return PLDM completion code
     */
    int handlePlatformEvent(pldm_tid_t tid, uint16_t eventId,
                            uint8_t eventClass, const uint8_t* eventData,
                            size_t eventDataSize);

    /** @brief Set available state of terminus for pldm request.
     *
     *  @param[in] tid - terminus ID
     *  @param[in] state - Terminus available state for PLDM request messages
     */
    void updateAvailableState(pldm_tid_t tid, Availability state)
    {
        availableState[tid] = state;
    };

    /** @brief Get available state of terminus for pldm request.
     *
     *  @param[in] tid - terminus ID
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
    /** @brief Helper method to process the PLDM Numeric sensor event class
     *
     *  @param[in] tid - tid where the event is from
     *  @param[in] sensorId - Sensor ID which is the source of event
     *  @param[in] sensorData - Numeric sensor event data
     *  @param[in] sensorDataLength - event data length
     *
     *  @return PLDM completion code
     */
    int processNumericSensorEvent(pldm_tid_t tid, uint16_t sensorId,
                                  const uint8_t* sensorData,
                                  size_t sensorDataLength);

    /** @brief Helper method to process the PLDM CPER event class
     *
     *  @param[in] tid - tid where the event is from
     *  @param[in] eventId - Event ID which is the source of event
     *  @param[in] eventData - CPER event data
     *  @param[in] eventDataSize - event data length
     *
     *  @return PLDM completion code
     */
    int processCperEvent(pldm_tid_t tid, uint16_t eventId,
                         const uint8_t* eventData, const size_t eventDataSize);

    /** @brief Helper method to create CPER dump log
     *
     *  @param[in] dataType - CPER event data type
     *  @param[in] dataPath - CPER event data fault log file path
     *  @param[in] terminusName - Terminus name which creates CPER event
     *
     *  @return PLDM completion code
     */
    int createCperDumpEntry(const std::string& dataType,
                            const std::string& dataPath,
                            const std::string& terminusName);

    /** @brief Reference of terminusManager */
    TerminusManager& terminusManager;

    /** @brief List of discovered termini */
    TerminiMapper& termini;

    /** @brief Available state for pldm request of terminus */
    std::unordered_map<pldm_tid_t, Availability> availableState;
};
} // namespace platform_mc
} // namespace pldm
