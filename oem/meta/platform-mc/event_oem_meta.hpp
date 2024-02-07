#pragma once

#define PLDM_OEM_EVENT_CLASS_0xFB 0xFB

#include "common/types.hpp"
#include "requester/configuration_discovery_handler.hpp"

namespace pldm
{
namespace platform_mc
{
namespace oem_meta
{

int processOemMetaEvent(
    tid_t tid, const uint8_t* eventData, size_t eventDataSize,
    const std::map<std::string, MctpEndpoint>& configurations);

} // namespace oem_meta
} // namespace platform_mc
} // namespace pldm
