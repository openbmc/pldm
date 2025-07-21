#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/utils.hpp"

namespace pldm::responder::oem_meta
{

enum class EventType : uint8_t
{
    CPU_THERMAL_TRIP = 0x0,
    HSC_OCP = 0x1,
    P12V_STBY_UV = 0x2,
    PMALERT_ASSERTION = 0x3,
    FAST_PROCHOT_ASSERTION = 0x4,
    FRB3_TIMER_EXPIRED = 0x5,
    POWER_ON_SEQUENCE_FAILURE = 0x6,
    DIMM_PMIC_ERROR = 0x7,
    ADDC_DUMP = 0x8,
    BMC_COMES_OUT_OF_COLD_RESET = 0x9,
    BIOS_FRB12_WATCHDOG_TIMER_EXPIRED = 0xa,
    BIC_POWER_FAILURE = 0xb,
    CPU_POWER_FAILURE = 0xc,
    V_BOOT_FAILURE = 0xd,
    BMC_IS_REQUESTED_TO_REBOOT = 0xe,
    CHASSIS_POWER_ON_IS_INITIATED_BY_NIC_INSERTION = 0xf,
    BLADE_IS_POWERED_CYCLE_BY_BLADE_BUTTON = 0x10,
    CHASSIS_IS_POWERED_CYCLE_BY_SLED_BUTTON = 0x11,
    HSC_FAULT = 0x12,
    SYS_THROTTLE = 0x13,
    VR_FAULT = 0x14,
    SYSTEM_MANAGEMENT_ERROR = 0x15,
    POST_COMPLETED = 0x16,
    FAN_ERROR = 0x17,
    HDT_PRSNT_ASSERTION = 0x18,
    PLTRST_ASSERTION = 0x19,
    APML_ALERT_ASSERTION = 0x1a,
    CXL1_HB = 0x1b,
    CXL2_HB = 0x1c,
    POST_STARTED = 0x1d,
    POST_ENDED = 0x1e,
    PROCHOT_IS_TRIGGERED_DUE_TO_SD_SENSOR_READING_EXCEED_UCR = 0x1f,
    FRB2_WDT_HARD_RST = 0x20,
    FRB2_WDT_PWR_DOWN = 0x21,
    FRB2_WDT_PWR_CYCLE = 0x22,
    OS_LOAD_WDT_EXPIRED = 0x23,
    OS_LOAD_WDT_HARD_RST = 0x24,
    OS_LOAD_WDT_PWR_DOWN = 0x25,
    OS_LOAD_WDT_PWR_CYCLE = 0x26,
    MTIA_FAULT = 0x27,
    POST_TIMEOUTED = 0x28,
};

enum class EventAssert : uint8_t
{
    EVENT_DEASSERTED = 0x0,
    EVENT_ASSERTED = 0x1,
};

struct VRSource
{
    std::string netName;
    bool isPMBus;
};

struct FaultDesc
{
    std::string eventType;
    uint8_t index;
    std::string name;
    std::string object_path;
    std::string eventName;
};

struct FaultData
{
    uint8_t detail1;
    uint8_t detail2;
};

/** @class EventLogHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to parse Event logs from BIC and add SEL
 */
class EventLogHandler : public FileHandler
{
  public:
    EventLogHandler() = delete;
    EventLogHandler(const EventLogHandler&) = delete;
    EventLogHandler(EventLogHandler&&) = delete;
    EventLogHandler& operator=(const EventLogHandler&) = delete;
    EventLogHandler& operator=(EventLogHandler&&) = delete;

    explicit EventLogHandler(pldm_tid_t tid) : tid(tid) {}

    ~EventLogHandler() = default;

    /** @brief Method to parse event log from eventList
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    int write(const message& data);

  private:
    /** @brief The terminus ID of the message source*/
    pldm_tid_t tid = 0;

    const std::string handleEventType(const message& data,
                                      const std::string& slotNumber) const;

    void addSystemEventLogAndJournal(const message& data,
                                     const std::string& event,
                                     const std::string& slotNum) const;
};

} // namespace pldm::responder::oem_meta
