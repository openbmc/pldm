#pragma once

#include "common/utils.hpp"
#include "oem_meta_file_io_by_type.hpp"
#include "requester/configuration_discovery_handler.hpp"

namespace pldm::responder::oem_meta
{
/** @class PostCodeHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to store incoming postcode
 */
class PostCodeHandler : public FileHandler
{
  public:
    PostCodeHandler(pldm_tid_t tid,
                    const std::map<std::string, MctpEndpoint>& configurations) :
        tid(tid),
        configurations(configurations)
    {}

    ~PostCodeHandler() = default;

    /** @brief Method to store postcode list
     *  @param[in] data - post code
     *  @return  PLDM status code
     */
    int write(const message& data);

    /** @brief Method to read postcode list
     *  @param[in] data - post code
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
