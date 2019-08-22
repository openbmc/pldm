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
    PelHandler(uint32_t newfileHandle, uint32_t newOffset, uint32_t newLength,
               uint64_t newAddress, fs::path newPath) :
        FileHandler(newfileHandle, newOffset, newLength, newAddress, newPath)
    {
    }

    int writeFromMemory();

    /** @brief method to store a pel file in tempfs and send
     *  d-bus notification to pel daemon that it is ready for consumption
     */
    virtual int storePel();

    /** @brief PelHandler destructor
     */
    ~PelHandler()
    {
    }

  protected:
    std::string pelFileName; //!< the pel file name in tempfs
};

} // namespace oem_file_type

} // namespace responder

} // namespace pldm
