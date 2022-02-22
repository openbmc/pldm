#include "terminus.hpp"

#include "libpldm/platform.h"

#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

Terminus::Terminus(pldm_tid_t tid, uint64_t supportedTypes) :
    initalized(false), tid(tid), supportedTypes(supportedTypes)
{}

bool Terminus::doesSupport(uint8_t type)
{
    return supportedTypes.test(type);
}
} // namespace platform_mc
} // namespace pldm
