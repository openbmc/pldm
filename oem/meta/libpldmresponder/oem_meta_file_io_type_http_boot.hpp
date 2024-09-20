#pragma once

#include "common/utils.hpp"
#include "oem_meta_file_io_by_type.hpp"
#include "requester/configuration_discovery_handler.hpp"

namespace pldm::responder::oem_meta
{

class HttpBootHandler : public FileHandler
{
  public:
    HttpBootHandler(pldm_tid_t tid,
                    const std::map<std::string, MctpEndpoint>& configurations) :
        tid(tid),
        configurations(configurations)
    {}

    ~HttpBootHandler() = default;

    int read(struct pldm_oem_meta_file_io_read_resp* data);

    int write(const message& data)
    {
        (void)data; // Unused
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

  private:
    /** @brief The terminus ID of the message source*/
    pldm_tid_t tid = 0;

    /** @brief Get existing configurations with MctpEndpoint*/
    const std::map<configDbusPath, MctpEndpoint>& configurations;
};

} // namespace pldm::responder::oem_meta
