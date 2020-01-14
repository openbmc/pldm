#pragma once

#include "file_io_by_type.hpp"

namespace pldm
{
namespace responder
{

/** @class DumpHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  handle the dump offload/streaming from host to the destination via bmc
 */
class DumpHandler : public FileHandler
{
  public:
    /** @brief DumpHandler constructor
     */
    DumpHandler(uint32_t fileHandle) : FileHandler(fileHandle)
    {
    }

    virtual int writeFromMemory(uint32_t /*offset*/, uint32_t /*length*/,
                                uint64_t /*address*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    virtual int readIntoMemory(uint32_t /*offset*/, uint32_t& /*length*/,
                               uint64_t /*address*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    virtual int read(uint32_t /*offset*/, uint32_t& /*length*/,
                     Response& /*response*/)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    virtual int processNewFileNotification(uint32_t length);

    /** @brief DumpHandler destructor
     */
    ~DumpHandler()
    {
    }
};

} // namespace responder
} // namespace pldm
