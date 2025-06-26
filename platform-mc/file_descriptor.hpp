#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"
#include "numeric_sensor.hpp"
#include "requester/handler.hpp"

#include <libpldm/platform.h>
#include <libpldm/pldm.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/PLDM/File/server.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace platform_mc
{

class TerminusManager;

using FileInterface = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PLDM::server::File>;

using namespace pldm::pdr;
using namespace pldm::file_transfer;

enum SocketIndex
{
    SERVER_IDX = 0,
    CLIENT_IDX = 1,
    SOCKET_CNT
};

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
    EntityInfo entityInfo;
    FileID identifier;
    FileID supDirIdentifier;
    FileSize maxSize;
    FDCount maxFdCount;
    std::string oemClassName;
    TerminusManager& terminusManager;

    mutable std::shared_ptr<NumericSensor> sizeSensor;

    // TODO: support multiple socket pairs based on
    // FileMaximumFileDescriptorCount
    /** @brief A socket pair to stream file data out upon an Open call.*/
    int sockets[SOCKET_CNT] = {-1, -1};

    /** @brief Coroutine handler for tasks.*/
    std::optional<std::pair<exec::async_scope, std::optional<int>>> taskHandle;

    /** @brief Set value to D-Bus Purpose property from FileClassification
     * field specified in the PDR
     *
     *  @param[in] classification - FileClassification field value
     */
    void setPurpose(FileClassification classification);

    /** @brief Write data of certain length to a unix socket
     *
     *  @param[in] dataSize - Data length to be written
     *  @param[in] data - Pointer to the array of data
     *  @param[in] socket - The socket fd
     *  @param[in] maxfd - Highest-num fd of all the fd set. maxfd + 1 will be
     *                     passed as nfds argument to select()
     *
     *  @return coroutine task returning int
     */
    exec::task<int> writeDataToSocket(
        size_t dataSize, const uint8_t* const data, int socket, int maxFd);

    /** @brief Call DfRead(s) to read a section of certain length to a buffer.
     *  With reponse data from each part, write it to the socket pair
     *
     *  @param[in] fd - fd to a session of a PLDM file open with DfOpen
     *  @param[in] offset - The desired section offset
     *  @param[in] length - The desired section length
     *  @param[in,out] readBuffer - The output buffer
     *
     *  @return coroutine task returning int
     */
    exec::task<int> readFileBySection(DfOpenFD fd, SectionOffset offset,
                                      SectionOffset length,
                                      std::vector<uint8_t>& readBuffer);

    /** @brief Read a certain length of data based on file type
     *
     *  @param[in] fd - fd to a session of a PLDM file open with DfOpen
     *  @param[in] offset - The offset to start reading at
     *  @param[in] readLength - The desired length to read from the file
     *
     *  @return coroutine task returning int
     */
    exec::task<int> readFileByLength(DfOpenFD fd, size_t offset,
                                     size_t readLength);

    /** @brief Open, read and close the actual file and write to the socket pair
     * as a request from the Open() method to the D-Bus file object
     *
     *  @param[in] fd - fd to a session of a PLDM file open with DfOpen
     *  @param[in] offset - The offset to start reading at
     *  @param[in] readSize - The desired length to read from the file
     *
     *  @return coroutine task returning int
     */
    exec::task<int> readTask(size_t offset, size_t length, bool exclusivity);

    /** @brief Override the handler of D-Bus Open() method that returns a
     * socket, and before that, spawn a task to interact with the actual file
     * and write data to the socket
     *
     *  @param[in] offset - The offset to start reading at
     *  @param[in] readSize - The desired length to read from the file
     *  @param[in] exclusivity - Whether to call DfOpen for an exclusive session
     *
     *  @return coroutine task returning int
     */
    sdbusplus::message::unix_fd open(size_t offset, size_t length,
                                     bool exclusivity) override;

    /** @brief Override the handler of D-Bus call to get Size property
     *
     *  @return current file size
     */
    size_t size() const override
    {
        return getFileSize();
    }

    /** @brief Get current file size from the current reading of the associated
     * File Size Monitoring sensor
     *
     *  @return current file size
     */
    FileSize getFileSize() const;
};
} // namespace platform_mc
} // namespace pldm
