#include "terminus.hpp"

#include "platform.h"

#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

Terminus::Terminus(mctp_eid_t _eid, uint8_t _tid, uint64_t supportedTypes) :
    _eid(_eid), _tid(_tid), supportedTypes(supportedTypes)
{}

bool Terminus::doesSupport(uint8_t type)
{
    return (supportedTypes.value & (1 << type));
}
} // namespace platform_mc
} // namespace pldm
