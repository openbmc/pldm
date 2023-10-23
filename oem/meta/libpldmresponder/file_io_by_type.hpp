#pragma once
#include <cstdint>
#include <vector>

namespace pldm
{
namespace responder
{
namespace oem_meta
{

/**
 *  @class FileHandler
 *
 *  Base class to handle read/write of all oem file types
 */
class FileHandler
{
  public:
    virtual int write(const std::vector<uint8_t>& data) = 0;
    virtual int read(const std::vector<uint8_t>& data) = 0;
    virtual ~FileHandler() {}
};

} // namespace oem_meta

} // namespace responder
} // namespace pldm
