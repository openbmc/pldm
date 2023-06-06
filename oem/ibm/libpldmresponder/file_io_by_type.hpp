#pragma once

#include "oem_ibm_handler.hpp"
#include "pldmd/pldm_resp_interface.hpp"

#include <libpldm/file_io.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/timer.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/exception.hpp>
#include <sdeventplus/source/io.hpp>
#include <sdeventplus/source/signal.hpp>
#include <sdeventplus/source/time.hpp>
#include <stdplus/signal.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <vector>
namespace pldm
{

namespace responder
{
using namespace sdeventplus;
using namespace sdeventplus::source;
constexpr auto clockId = sdeventplus::ClockId::RealTime;
using Timer = Time<clockId>;
using Clock = Clock<clockId>;

class FileHandler;
namespace dma
{
class DMA;
} // namespace dma

struct ResponseHdr
{
    uint8_t instance_id;
    uint8_t command;
    pldm::response_api::Transport* respInterface;
    std::shared_ptr<FileHandler> functionPtr = nullptr;
    int key;
};

namespace fs = std::filesystem;

/**
 *  @class FileHandler
 *
 *  Base class to handle read/write of all oem file types
 */
class FileHandler
{
  protected:
    /** @brief method to send response to host after completion of DMA operation
     * @param[in] responseHdr - contain response related data
     * @param[in] rStatus - operation status either success/fail/not suppoted.
     * @param[in] length - length to be read/write mentioned by Host
     */
    virtual void dmaResponseToHost(const ResponseHdr& responseHdr,
                                   const pldm_completion_codes rStatus,
                                   uint32_t length);

    /** @brief method to send response to host after completion of DMA operation
     * @param[in] responseHdr - contain response related data
     * @param[in] rStatus - operation status either success/fail/not suppoted.
     * @param[in] length - length to be read/write mentioned by Host
     */
    virtual void dmaResponseToHost(const ResponseHdr& responseHdr,
                                   const pldm_fileio_completion_codes rStatus,
                                   uint32_t length);

    /** @brief method to delete all shared pointer object
     * @param[in] responseHdr - contain response related data
     * @param[in] xdmaInterface - interface to transfer data between BMc and
     * Host
     */
    virtual void
        deleteAIOobjects(const std::shared_ptr<dma::DMA>& xdmaInterface,
                         const ResponseHdr& responseHdr);

  public:
    /** @brief Method to write an oem file type from host memory. Individual
     *  file types need to override this method to do the file specific
     *  processing
     *  @param[in] offset - offset to read/write
     *  @param[in] length - length to be read/write mentioned by Host
     *  @param[in] address - DMA address
     *  @param[in] oemPlatformHandler - oem handler for PLDM platform related
     *                                  tasks
     *  @return PLDM status code
     */
    virtual void writeFromMemory(uint32_t offset, uint32_t length,
                                 uint64_t address,
                                 oem_platform::Handler* oemPlatformHandler,
                                 ResponseHdr& responseHdr,
                                 sdeventplus::Event& event) = 0;

    /** @brief Method to read an oem file type into host memory. Individual
     *  file types need to override this method to do the file specific
     *  processing
     *  @param[in] offset - offset to read
     *  @param[in/out] length - length to be read mentioned by Host
     *  @param[in] address - DMA address
     *  @param[in] oemPlatformHandler - oem handler for PLDM platform related
     *                                  tasks
     *  @return PLDM status code
     */
    virtual void readIntoMemory(uint32_t offset, uint32_t& length,
                                uint64_t address,
                                oem_platform::Handler* oemPlatformHandler,
                                ResponseHdr& responseHdr,
                                sdeventplus::Event& event) = 0;

    /** @brief Method to read an oem file type's content into the PLDM response.
     *  @param[in] offset - offset to read
     *  @param[in/out] length - length to be read
     *  @param[in] response - PLDM response
     *  @param[in] oemPlatformHandler - oem handler for PLDM platform related
     *                                  tasks
     *  @return PLDM status code
     */
    virtual int read(uint32_t offset, uint32_t& length, Response& response,
                     oem_platform::Handler* oemPlatformHandler) = 0;

    /** @brief Method to write an oem file by type
     *  @param[in] buffer - buffer to be written to file
     *  @param[in] offset - offset to write to
     *  @param[in/out] length - length to be written
     *  @param[in] oemPlatformHandler - oem handler for PLDM platform related
     *                                  tasks
     *  @return PLDM status code
     */
    virtual int write(const char* buffer, uint32_t offset, uint32_t& length,
                      oem_platform::Handler* oemPlatformHandler) = 0;

    virtual int fileAck(uint8_t fileStatus) = 0;

    /** @brief Method to process a new file available notification from the
     *  host. The bmc can chose to do different actions based on the file type.
     *
     *  @param[in] length - size of the file content to be transferred
     *
     *  @return PLDM status code
     */
    virtual int newFileAvailable(uint64_t length) = 0;

    /** @brief Method to read an oem file type's content into the PLDM response.
     *  @param[in] filePath - file to read from
     *  @param[in] offset - offset to read
     *  @param[in/out] length - length to be read
     *  @param[in] response - PLDM response
     *  @return PLDM status code
     */
    virtual int readFile(const std::string& filePath, uint32_t offset,
                         uint32_t& length, Response& response);

    /** @brief Method to do the file content transfer ove DMA between host and
     *  bmc. This method is made virtual to be overridden in test case. And need
     *  not be defined in other child classes
     *
     *  @param[in] path - file system path  where read/write will be done
     *  @param[in] upstream - direction of DMA transfer. "false" means a
     *                        transfer from host to BMC
     *  @param[in] offset - offset to read/write
     *  @param[in/out] length - length to be read/write mentioned by Host
     *  @param[in] address - DMA address
     *
     *  @return PLDM status code
     */
    virtual void transferFileData(const fs::path& path, bool upstream,
                                  uint32_t offset, uint32_t& length,
                                  uint64_t address, ResponseHdr& responseHdr,
                                  sdeventplus::Event& event);

    virtual void transferFileData(int fd, bool upstream, uint32_t offset,
                                  uint32_t& length, uint64_t address,
                                  ResponseHdr& responseHdr,
                                  sdeventplus::Event& event);

    virtual void transferFileDataToSocket(int fd, uint32_t& length,
                                          uint64_t address,
                                          ResponseHdr& responseHdr,
                                          sdeventplus::Event& event);

    /** @brief method to do necessary operation according different
     *  file type and being call when data transfer completed.
     *
     *  @param[in] IsWriteToMemOp - type of operation to decide what operation
     * needs to be done after data transfer.
     */
    virtual void postDataTransferCallBack(bool IsWriteToMemOp) = 0;

    /** @brief Constructor to create a FileHandler object
     */
    FileHandler(uint32_t fileHandle) : fileHandle(fileHandle) {}

    /** FileHandler destructor
     */
    virtual ~FileHandler() {}

  protected:
    uint32_t fileHandle; //!< file handle indicating name of file or invalid
};

/** @brief Method to create individual file handler objects based on file type
 *
 *  @param[in] fileType - type of file
 *  @param[in] fileHandle - file handle
 */

std::unique_ptr<FileHandler> getHandlerByType(uint16_t fileType,
                                              uint32_t fileHandle);

/** @brief Method to create shared file handler objects based on file type
 *
 *  @param[in] fileType - type of file
 *  @param[in] fileHandle - file handle
 */
std::shared_ptr<FileHandler> getSharedHandlerByType(uint16_t fileType,
                                                    uint32_t fileHandle);
} // namespace responder
} // namespace pldm
