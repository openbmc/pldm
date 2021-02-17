#include "file_io_type_progress_src.hpp"

namespace pldm
{

namespace responder
{

int ProgressCodeHandler::writeFromMemory(uint32_t /*offset*/,uint32_t /*length*/,uint64_t /*address*/,oem_platform::Handler* /*oemPlatformHandler*/)
{
    // pldm requestor will do a dma read
    return 0;
}

int ProgressCodeHandler::write(const char* /*buffer*/, uint32_t /*offset*/, uint32_t& /*length*/,
                      oem_platform::Handler* /*oemPlatformHandler*/)
{
    return 0;

}

int ProgressCodeHandler::newFileAvailable(uint64_t length)
{
    pendingSize = length;
    return PLDM_SUCCESS;
}

} // namespace responder
} // namespace pldm