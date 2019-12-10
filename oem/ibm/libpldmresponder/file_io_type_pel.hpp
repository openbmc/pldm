#pragma once

#include "file_io_by_type.hpp"

namespace pldm
{
namespace responder
{

using namespace pldm::responder::dma;

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
    PelHandler(uint32_t fileHandle) : FileHandler(fileHandle)
    {
    }

    virtual int writeFromMemory(uint32_t offset, uint32_t length,
                                uint64_t address);
    virtual int readIntoMemory(uint32_t offset, uint32_t& length,
                               uint64_t address);
    virtual int read(uint32_t offset, uint32_t& length, Response& response);

    virtual int fileAck(uint32_t fileHandle);

    /** @brief method to store a pel file in tempfs and send
     *  d-bus notification to pel daemon that it is ready for consumption
     *
     *  @param[in] pelFileName - the pel file path
     */
    virtual int storePel(std::string&& pelFileName);

    /** @brief method to send acknowledgement via d-bus notification to host
     *  for a pel file created in tempfs
     *
     *  @param[in] pelID - the pel ID
     */
    virtual int fileAckPel(uint32_t pelID);

    /** @brief PelHandler destructor
     */
    ~PelHandler()
    {
    }
};

} // namespace responder
} // namespace pldm
