#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/utils.hpp"

namespace pldm::responder::oem_meta
{
enum class EventAssert : uint8_t
{
    EVENT_DEASSERTED = 0x00,
    EVENT_ASSERTED = 0x01,
};

struct VRSource
{
    std::string netName;
    bool isPMBus;
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

    explicit EventLogHandler(
        pldm_tid_t tid,
        const std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>&
            configurations) : tid(tid), configurations(configurations)
    {}

    ~EventLogHandler() = default;

    /** @brief Method to parse event log from eventList
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    int write(const message& data);

  private:
    /** @brief The terminus ID of the message source*/
    pldm_tid_t tid = 0;

    /** @brief Get existing configurations with MctpEndpoint*/
    const std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>&
        configurations;
};

} // namespace pldm::responder::oem_meta
