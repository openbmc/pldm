#pragma once

#include "file_io_by_type.hpp"
#include <fstream>
#include <filesystem>
#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{

/** @class ReconfigLoopHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to handle the reconfig loop notification from Hostboot by
 *  creating a marker file at /run/openbmc/reconfigloop.
 */
class ReconfigLoopHandler : public FileHandler
{
  public:
    ReconfigLoopHandler(uint32_t fileHandle) : FileHandler(fileHandle) {}

    int newFileAvailable(uint64_t /*length*/) override
    {
        static constexpr auto reconfigLoopFile =
            "/run/openbmc/reconfigloop";

        std::ofstream file(reconfigLoopFile);
        if (!file)
        {
            error("Failed to create reconfig loop marker file '{PATH}'",
                  "PATH", reconfigLoopFile);
            return PLDM_ERROR;
        }
        return PLDM_SUCCESS;
    }

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


    int read(uint32_t /*offset*/, uint32_t& /*length*/,
             Response& /*response*/,
             oem_platform::Handler* /*oemPlatformHandler*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }


    int write(const char* /*buffer*/, uint32_t /*offset*/,
              uint32_t& /*length*/,
              oem_platform::Handler* /*oemPlatformHandler*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }


    int fileAck(uint8_t /*fileStatus*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }


    int fileAckWithMetaData(uint8_t /*fileStatus*/,
                            uint32_t /*metaDataValue1*/,
                            uint32_t /*metaDataValue2*/,
                            uint32_t /*metaDataValue3*/,
                            uint32_t /*metaDataValue4*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }


    int newFileAvailableWithMetaData(uint64_t /*length*/,
                                     uint32_t /*metaDataValue1*/,
                                     uint32_t /*metaDataValue2*/,
                                     uint32_t /*metaDataValue3*/,
                                     uint32_t /*metaDataValue4*/) override
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    
    ~ReconfigLoopHandler() {}
};

} // namespace responder
} // namespace pldm