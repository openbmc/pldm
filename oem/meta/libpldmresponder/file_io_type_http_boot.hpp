#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/requester/configuration_discovery_handler.hpp"

namespace pldm::responder::oem_meta
{
/** @class PostCodeHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to store incoming postcode
 */
class HttpBootHandler : public FileHandler
{
  public:
    HttpBootHandler() {}

    ~HttpBootHandler() = default;

    /** @brief Method to parse read file IO for Http Boot command (type: 0x03)
     *and returning the Http boot certification stored in BMC.
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    int read(struct pldm_oem_meta_file_io_read_resp* data);

    /** @brief Method to write data to BMC
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    int write(const message& data)
    {
        (void)data; // Unused
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
};

} // namespace pldm::responder::oem_meta
