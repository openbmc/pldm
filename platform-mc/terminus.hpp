#pragma once

#include "libpldm/platform.h"

#include "common/types.hpp"
#include "requester/handler.hpp"

#include <sdbusplus/server/object.hpp>
#include <sdeventplus/event.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/server.hpp>

using namespace pldm::pdr;

namespace pldm
{
namespace platform_mc
{
/**
 * @brief Terminus
 *
 * Terminus class holds the EID, TID, supportedPLDMType or PDRs which are
 * needed by other manager class for sensor monitoring and control.
 */
class Terminus
{
  public:
    Terminus(mctp_eid_t _eid, uint8_t _tid, uint64_t supportedPLDMTypes);

    /** @brief Check if the terminus supports the PLDM type message
     *
     *  @param[in] type - PLDM Type
     */
    bool doesSupport(uint8_t type);

    /** @brief The getter to return terminus's EID */
    mctp_eid_t eid()
    {
        return _eid;
    }

    /** @brief The getter to return terminus's TID */
    uint8_t tid()
    {
        return _tid;
    }

    /** @brief A list of PDRs fetched from Terminus */
    std::vector<std::vector<uint8_t>> pdrs{};

  private:
    mctp_eid_t _eid;
    uint8_t _tid;
    uint64_t supportedTypes;
};
} // namespace platform_mc
} // namespace pldm
