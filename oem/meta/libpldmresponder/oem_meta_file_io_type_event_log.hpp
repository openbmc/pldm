#pragma once

#include "common/utils.hpp"
#include "oem_meta_file_io_by_type.hpp"
#include "requester/configuration_discovery_handler.hpp"

namespace pldm::responder::oem_meta
{
enum eventAssert : uint8_t
{
    EVENT_DEASSERTED = 0x00,
    EVENT_ASSERTED = 0x01,
};

/** @class EventLogHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to parse Event logs from BIC and add SEL
 */
class EventLogHandler : public FileHandler
{
  public:
    EventLogHandler(pldm_tid_t tid,
                    const std::map<std::string, MctpEndpoint>& configurations) :
        tid(tid),
        configurations(configurations)
    {}

    ~EventLogHandler() = default;

    /** @brief Method to parse event log from eventList
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    int write(const message& data);

    /** @brief Method to read event log from BIC
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    int read(const message&)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

  private:
    /** @brief Parse object path to get correspond slot number
     *  @param[in] slot - slot number
     */
    void parseObjectPathToGetSlot(uint64_t& slot);

    /** @brief The terminus ID of the message source*/
    pldm_tid_t tid = 0;

    /** @brief Get existing configurations with MctpEndpoint*/
    const std::map<configDbusPath, MctpEndpoint>& configurations;
};

} // namespace pldm::responder::oem_meta
