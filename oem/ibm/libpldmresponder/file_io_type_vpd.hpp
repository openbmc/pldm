#pragma once

#include "file_io_by_type.hpp"

namespace pldm
{
namespace responder
{
using vpdFileType = uint16_t;
/** @class keywordFileHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to read #D keyword file
 */
class keywordHandler : public FileHandler
{
  public:
    /** @brief Handler constructor
     */
    keywordHandler(uint32_t fileHandle, uint16_t /* fileType */) :
        FileHandler(fileHandle)
    {}
    keywordHandler(const keywordHandler&) = delete;
    keywordHandler& operator=(const keywordHandler&) = delete;
    keywordHandler(keywordHandler&&) = default;
    keywordHandler& operator=(keywordHandler&&) = default;

    int writeFromMemory(uint32_t /*offset*/, uint32_t /*length*/,
                        uint64_t /*address*/,
                        oem_platform::Handler* /*oemPlatformHandler*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    int readIntoMemory(uint32_t /*offset*/, uint32_t /*length*/,
                       uint64_t /*address*/,
                       oem_platform::Handler* /*oemPlatformHandler*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    int read(uint32_t offset, uint32_t& length, Response& response,
             oem_platform::Handler* /*oemPlatformHandler*/) override;
    int write(const char* /*buffer*/, uint32_t /*offset*/, uint32_t& /*length*/,
              oem_platform::Handler* /*oemPlatformHandler*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    int fileAck(uint8_t /*fileStatus*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    int newFileAvailable(uint64_t /*length*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    int fileAckWithMetaData(uint8_t /*fileStatus*/, uint32_t /*metaDataValue1*/,
                            uint32_t /*metaDataValue2*/,
                            uint32_t /*metaDataValue3*/,
                            uint32_t /*metaDataValue4*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    int newFileAvailableWithMetaData(
        uint64_t /*length*/, uint32_t /*metaDataValue1*/,
        uint32_t /*metaDataValue2*/, uint32_t /*metaDataValue3*/,
        uint32_t /*metaDataValue4*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    /** @brief keywordHandler destructor
     */
    ~keywordHandler() override {}
};
} // namespace responder
} // namespace pldm
