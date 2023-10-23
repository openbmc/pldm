#pragma once

#include "common/utils.hpp"
#include "file_io_by_type.hpp"

namespace pldm
{
namespace responder
{
namespace oem_meta
{
/** @class PostCodeHandler
 *
 *  @brief Inherits and implements FileHandler. This class is used
 *  to store incoming postcode
 */
class PostCodeHandler : public FileHandler
{
  public:
    PostCodeHandler(uint8_t tid, const std::map<uint8_t, int>& tidToSlotMap) :
        tid(tid), tidToSlotMap(tidToSlotMap)
    {}

    /** @brief Method to store postcode list
     *  @param[in] data - post code
     *  @return  PLDM status code
     */
    int write(const std::vector<uint8_t>& data);

    int read([[maybe_unused]] const std::vector<uint8_t>& data)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

  private:
    uint8_t tid = 0;
    const std::map<uint8_t, int> tidToSlotMap;
};

} // namespace oem_meta
} // namespace responder
} // namespace pldm
