#pragma once

#include "file_io_by_type.hpp"

namespace pldm
{
namespace responder
{
using DumpEntryInterface = std::string;

/** @class DumpHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  handle the dump offload/streaming from host to the destination via bmc
 */
class DumpHandler : public FileHandler
{
  public:
    DumpHandler(const DumpHandler&) = delete;
    DumpHandler& operator=(const DumpHandler&) = delete;
    DumpHandler(DumpHandler&&) = default;
    DumpHandler& operator=(DumpHandler&&) = default;
    /** @brief DumpHandler constructor
     */
    DumpHandler(uint32_t fileHandle, uint16_t fileType) :
        FileHandler(fileHandle), dumpType(fileType)
    {}

    int writeFromMemory(uint32_t offset, uint32_t length, uint64_t address,
                        oem_platform::Handler* /*oemPlatformHandler*/) override;

    int readIntoMemory(uint32_t offset, uint32_t length, uint64_t address,
                       oem_platform::Handler* /*oemPlatformHandler*/) override;

    int read(uint32_t offset, uint32_t& length, Response& response,
             oem_platform::Handler* /*oemPlatformHandler*/) override;

    int write(const char* buffer, uint32_t offset, uint32_t& length,
              oem_platform::Handler* /*oemPlatformHandler*/) override;

    int newFileAvailable(uint64_t length) override;

    int fileAck(uint8_t fileStatus) override;

    int fileAckWithMetaData(uint8_t /*fileStatus*/, uint32_t /*metaDataValue1*/,
                            uint32_t /*metaDataValue2*/,
                            uint32_t /*metaDataValue3*/,
                            uint32_t /*metaDataValue4*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    int newFileAvailableWithMetaData(
        uint64_t length, uint32_t metaDataValue1, uint32_t /*metaDataValue2*/,
        uint32_t /*metaDataValue3*/, uint32_t /*metaDataValue4*/) override;

    std::string findDumpObjPath(uint32_t fileHandle);
    std::string getOffloadUri(uint32_t fileHandle);

    /** @brief DumpHandler destructor
     */
    ~DumpHandler() override {}

  private:
    static int fd;     //!< fd to manage the dump offload to bmc
    uint16_t dumpType; //!< type of the dump
};

} // namespace responder
} // namespace pldm
