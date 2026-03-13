#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/utils.hpp"

namespace pldm::responder::oem_meta
{

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
};

} // namespace pldm::responder::oem_meta
