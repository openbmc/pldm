#pragma once

#include "common/utils.hpp"
#include "oem_meta_file_io_by_type.hpp"
#include "pldmd/handler.hpp"
#include "requester/configuration_discovery_handler.hpp"

#include <libpldm/oem/meta/file_io.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::responder::oem_meta
{

class FileIOHandler : public CmdHandler
{
  public:
    FileIOHandler(pldm::utils::DBusHandler* dBusIntf,
                  pldm::ConfigurationDiscoveryHandler* configurationDiscovery) :
        dBusIntf(dBusIntf),
        configurationDiscovery(configurationDiscovery)
    {
        handlers.emplace(PLDM_OEM_META_FILEIO_CMD_WRITE_FILE,
                         [this](pldm_tid_t tid, const pldm_msg* request,
                                size_t payloadLength) {
            return this->writeFileIO(tid, request, payloadLength);
        });
    }

  private:
    /** @brief Handler for writeFileIO command
     *
     *  @param[in] tid - the device tid
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message
     *
     *  @return PLDM response message
     */
    Response writeFileIO(pldm_tid_t tid, const pldm_msg* request,
                         size_t payloadLength);

    std::unique_ptr<FileHandler> getHandlerByType(pldm_tid_t tid,
                                                  uint8_t fileIOType);

    /** @brief D-Bus Interface object*/
    const pldm::utils::DBusHandler* dBusIntf;

    /** @brief Configuration Discovery Object which stores
     * EntityManager's config
     */
    pldm::ConfigurationDiscoveryHandler* configurationDiscovery;
};

} // namespace pldm::responder::oem_meta
