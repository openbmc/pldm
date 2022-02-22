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
 * Terminus class holds the EID, TID, supported PLDM Type or PDRs which are
 * needed by other manager class for sensor monitoring and control.
 */
class Terminus
{
  public:
    Terminus(mctp_eid_t eid, PldmTID tid, uint64_t supportedPLDMTypes);

    /** @brief Check if the terminus supports the PLDM type message
     *
     *  @param[in] type - PLDM Type
     */
    bool doesSupport(uint8_t type);

    /** @brief The getter to return terminus's EID */
    mctp_eid_t getEid()
    {
        return eid;
    }

    /** @brief The getter to return terminus's TID */
    PldmTID getTid()
    {
        return tid;
    }

    /** @brief A list of PDRs fetched from Terminus */
    std::vector<std::vector<uint8_t>> pdrs{};

  private:
    mctp_eid_t eid;
    PldmTID tid;
    std::bitset<64> supportedTypes;
};
} // namespace platform_mc
} // namespace pldm
