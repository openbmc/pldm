#pragma once

#include "file_io_by_type.hpp"

#include <libpldm/oem/meta/file_io.h>

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
    HttpBootHandler() = default;
    HttpBootHandler(const HttpBootHandler&) = delete;
    HttpBootHandler(HttpBootHandler&&) = delete;
    HttpBootHandler& operator=(const HttpBootHandler&) = delete;
    HttpBootHandler& operator=(HttpBootHandler&&) = delete;

    ~HttpBootHandler() = default;

    /** @brief Method to parse read file IO for Http Boot command (type: 0x03)
     *         and returning the Http boot certification stored in BMC.
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    int read(struct pldm_oem_meta_file_io_read_resp* data) override;
};

} // namespace pldm::responder::oem_meta
