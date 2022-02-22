#include "terminus.hpp"

#include "libpldm/platform.h"

#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

Terminus::Terminus(pldm_tid_t tid, uint64_t supportedTypes) :
    initialized(false), tid(tid), supportedTypes(supportedTypes)
{}

bool Terminus::doesSupportType(uint8_t type)
{
    return supportedTypes.test(type);
}

bool Terminus::doesSupportCommand(uint8_t type, uint8_t command)
{
    if (!doesSupportType(type))
    {
        return false;
    }

    try
    {
        auto idx = type * (PLDM_MAX_CMDS_PER_TYPE / 8) + (command / 8);
        if (supportedCmds[idx].test(command % 8))
        {
            lg2::error(
                "doesSupportCommand PLDM type {TYPE} command {CMD} is supported by terminus {TID}",
                "TYPE", type, "CMD", command, "TID", getTid());
            return true;
        }
    }
    catch (const std::exception& e)
    {
        return false;
    }

    return false;
}

} // namespace platform_mc
} // namespace pldm
