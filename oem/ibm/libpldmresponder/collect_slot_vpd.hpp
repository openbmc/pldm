#pragma once

#include "common/utils.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "libpldmresponder/platform.hpp"

#include <string>

using namespace pldm::utils;
namespace pldm
{
namespace responder
{

/** @class SlotEnable
 *
 *  @brief This class performs the necessary operation in pldm for
 *         Slot Enable operation. That includes taking actions on the
 *         setStateEffecterStates calls from Host and also sending
 *         notification to inventory manager application
 */
class SlotEnable
{
  public:
    /** @brief Constructor to create an slot enable object
     *  @param[in] dBusIntf - D-Bus handler pointer
     */
    SlotEnable(const pldm::utils::DBusHandler* dBusIntf) : dBusIntf(dBusIntf)
    {
        fruPresenceMatch = nullptr;
    }

    /* @brief Method to set the oem platform handler in SlotEnable class */
    void setOemPlatformHandler(pldm::responder::oem_platform::Handler* handler);

    void sendStateSensorEvent(uint16_t sensorId,
                              enum sensor_event_class_states sensorEventClass,
                              uint8_t sensorOffset, uint8_t eventState,
                              uint8_t prevEventState);
    void createPresenceMatch(std::string ObjectPath, StateSetId statesetId);

    virtual ~SlotEnable()
    {}

  private:
    const pldm::utils::DBusHandler* dBusIntf; //!< D-Bus handler

    pldm::responder::oem_platform::Handler*
        oemPlatformHandler; //!< oem platform handler

    /** @brief D-Bus property changed signal match for image activation */
    std::unique_ptr<sdbusplus::bus::match::match> fruPresenceMatch;
};

} // namespace responder
} // namespace pldm
