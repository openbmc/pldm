#pragma once

#include "file_io_by_type.hpp"

namespace pldm
{

using namespace pldm::responder::dma;
namespace responder
{

namespace oem_file_type
{

/** @class LidHandler
 *  @brief Inherits and implements FileHandler. This class is used
 *  to read/write lid files
 */
class LidHandler : public FileHandler
{
  public:
    /** @brief LidHandler constructor
     */
    LidHandler(uint32_t newfileHandle, uint32_t newOffset, uint32_t newLength,
               uint64_t newAddress, fs::path newPath) :
        FileHandler(newfileHandle, newOffset, newLength, newAddress, newPath)
    {
    }

    int readIntoMemory();

    ~LidHandler()
    {
    }
};

/** @brief creates the lid file path from the filehandle
 *
 *  @param[in] fileHandle - file handle passed by Host
 *
 *  @return fs::path for the lid file
 */
fs::path createLidPath(uint32_t fileHandle);

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
