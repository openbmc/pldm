#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"
#include "numeric_sensor.hpp"
#include "requester/handler.hpp"

#include <libpldm/platform.h>
#include <libpldm/pldm.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Common/File/server.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

class TerminusManager;

using FileInterface = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Common::server::File>;

using namespace pldm::pdr;
using namespace pldm::file_transfer;

/**
 * @brief FileDescriptor
 *
 * This class represents a File Descriptor object introduced by File Descriptor
 * PDR to D-Bus.
 */
class FileDescriptor : virtual public FileInterface
{
  public:
    FileDescriptor(sdbusplus::bus_t& bus, const std::string& path,
                   const pldm_tid_t tid, const bool fileDisabled,
                   std::shared_ptr<pldm_platform_file_descriptor_pdr> pdr,
                   TerminusManager& terminusManager);

    ~FileDescriptor() = default;

  private:
    bool isDirectory = false;
    bool isRegular = false;
    bool exReadPermitted = false;
    pldm_tid_t tid;
    FileID identifier;
    FileID supDirIdentifier;
    FileSize maxSize;
    FDCount maxFdCount;
    std::string oemClassName;
    TerminusManager& terminusManager;

    mutable std::shared_ptr<NumericSensor> sizeSensor;

    // TODO: support multiple socket pairs based on
    // FileMaximumFileDescriptorCount
    int sockets[2];

    void setPurpose(const FileClassification& classification);
    exec::task<int> writeDataToSocket(const size_t& dataSize,
                                      const uint8_t* const data,
                                      const int& socket, const int& maxFd);
    exec::task<int> readFileToSocket(const FD& fd, const SectionOffset& offset,
                                     const SectionOffset& length,
                                     std::vector<uint8_t>& readBuffer);
    exec::task<int> readFile(const FD& fd, const size_t& offset,
                             const size_t& readSize);
    exec::task<int> readTask(const size_t& offset, const size_t& length,
                             bool exclusivity);
    sdbusplus::message::unix_fd open(size_t offset, size_t length,
                                     bool exclusivity) override;
    size_t size() const override
    {
        return getFileSize();
    }
    std::optional<FileSize> getFileSize() const;
    std::optional<std::pair<exec::async_scope, std::optional<int>>> taskHandle;
};
} // namespace platform_mc
} // namespace pldm
