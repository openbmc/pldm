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
 * The Terminus class provides APIs keeps data the  which is needed for sensor
 * monitoring and control.
 */
class Terminus
{
  public:
    Terminus(mctp_eid_t _eid, uint8_t _tid, uint64_t supportedPLDMTypes);
    bool doesSupport(uint8_t type);
    mctp_eid_t eid()
    {
        return _eid;
    }

    uint8_t tid()
    {
        return _tid;
    }

    std::vector<std::vector<uint8_t>> pdrs{};

  private:
    mctp_eid_t _eid;
    uint8_t _tid;
    uint64_t supportedTypes;
};
} // namespace platform_mc
} // namespace pldm
