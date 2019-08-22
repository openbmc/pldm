#pragma once

#include "file_io_by_type.hpp"

namespace pldm
{

using namespace pldm::responder::dma;
namespace responder
{

namespace oem_file_type
{

/** @class PelHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to read/write pels.
 */
class PelHandler : public FileHandler
{
  public:
    /** @brief PelHandler constructor
     */
    PelHandler(uint32_t fileHandle, uint32_t Offset, uint32_t Length,
               uint64_t Address) :
        FileHandler(fileHandle, Offset, Length, Address)
    {
    }

    int writeFromMemory();

    int readIntoMemory();

    /** @brief method to store a pel file in tempfs and send
     *  d-bus notification to pel daemon that it is ready for consumption
     *
     *  @param[in] pelFileName - the pel file path
     */
    virtual int storePel(const std::string& pelFileName);

    /** @brief PelHandler destructor
     */
    ~PelHandler()
    {
    }
};

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
