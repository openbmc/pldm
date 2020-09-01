#include "file_io_type_lid.hpp"

#include <fstream>

namespace pldm
{
namespace responder
{

int LidHandler::write(const std::string& filePath, const char* buffer,
                      uint32_t offset, uint32_t& length)
{
    std::ofstream stream(filePath,
                         std::ios::out | std::ios::app | std::ios::binary);
    stream.seekp(offset);
    stream.write(buffer, length);
    stream.flush();
    stream.close();

    return PLDM_SUCCESS;
}

int LidHandler::write(const char* buffer, uint32_t offset, uint32_t& length)
{
    return write(lidPath, buffer, offset, length);
}

} // namespace responder
} // namespace pldm
