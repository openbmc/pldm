#pragma once

#include "file_io_by_type.hpp"

#include <iostream>

namespace pldm
{

namespace responder
{

/** @class ProgressCodeHandler
 *
 * @brief Inherits and implemented FileHandler. This class is used
 * to read the Progress SRC's from the Host.
 */
class ProgressCodeHandler : public FileHandler
{
  public:
    /** @brief ProgressCodeHandler constructor
     */
    ProgressCodeHandler(uint32_t fileHandle) : FileHandler(fileHandle)
    {}

    virtual int writeFromMemory(uint32_t offset, uint32_t length,
                                uint64_t address,
                                oem_platform::Handler* oemPlatformHandler);

    virtual int write(const char* buffer, uint32_t offset, uint32_t& length,
                      oem_platform::Handler* oemPlatformHandler);

    virtual int readIntoMemory(uint32_t /*offset*/, uint32_t& /*length*/,
                               uint64_t /*address*/,
                               oem_platform::Handler* /*oemPlatformHandler*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    virtual int read(uint32_t /*offset*/, uint32_t& /*length*/, Response& /*response*/,
                     oem_platform::Handler* /*oemPlatformHandler*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    virtual int fileAck(uint8_t /*fileStatus*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    } 

    virtual int newFileAvailable(uint64_t length);

    /** @brief ProgressCodeHandler destructor
     */

    ~ProgressCodeHandler()
    {}

    protected:
        std::vector<char> progressCodeBuffer;
        uint64_t pendingSize;
};

} // namespace responder
} // namespace pldm