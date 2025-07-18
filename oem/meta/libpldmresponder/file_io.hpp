#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"
#include "oem/meta/requester/configuration_discovery_handler.hpp"
#include "pldmd/handler.hpp"

#include <libpldm/oem/meta/file_io.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::responder::oem_meta
{

class FileIOHandler : public CmdHandler
{
  public:
		FileIOHandler() = delete;
		FileIOHandler(const FileIOHandler&) = delete;
		FileIOHandler(FileIOHandler&&) = delete;
		FileIOHandler& operator=(const FileIOHandler&) = delete;
		FileIOHandler& operator=(FileIOHandler&&) = delete;
    virtual ~FileIOHandler() = default;

    explicit FileIOHandler(std::shared_ptr<
                      pldm::requester::oem_meta::ConfigurationDiscoveryHandler>
                      configurationDiscovery,
                  const pldm::utils::DBusHandler* dBusHandler) :
        configurationDiscovery(configurationDiscovery), dBusHandler(dBusHandler)
    {
        handlers.emplace(
            PLDM_OEM_META_FILE_IO_CMD_WRITE_FILE,
            [this](pldm_tid_t tid, const pldm_msg* request,
                   size_t payloadLength) {
                return this->writeFileIO(tid, request, payloadLength);
            });
        handlers.emplace(
            PLDM_OEM_META_FILE_IO_CMD_READ_FILE,
            [this](pldm_tid_t tid, const pldm_msg* request,
                   size_t payloadLength) {
                return this->readFileIO(tid, request, payloadLength);
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

    /** @brief Handler for readFileIO command
     *
     *  @param[in] tid - the device tid
     *  @param[in] request - pointer to PLDM request payload
     *  @param[in] payloadLength - length of the message
     *
     *  @return PLDM response message
     */
    Response readFileIO(pldm_tid_t tid, const pldm_msg* request,
                        size_t payloadLength);

    std::unique_ptr<FileHandler> getHandlerByType(pldm_tid_t tid,
                                                  FileIOType fileIOType);

    std::shared_ptr<pldm::requester::oem_meta::ConfigurationDiscoveryHandler>
        configurationDiscovery;

    [[maybe_unused]] const pldm::utils::DBusHandler* dBusHandler;
};

} // namespace pldm::responder::oem_meta
