#pragma once

#define PLDM_OEM_EVENT_CLASS_0xFB 0xFB

#include "common/types.hpp"
#include "requester/configuration_discovery_handler.hpp"

typedef struct _dimm_info
{
    uint8_t sled;
    uint8_t socket;
    uint8_t channel;
    uint8_t slot;
} _dimm_info;

namespace pldm
{
namespace platform_mc
{
namespace oem_meta
{
std::string to_hex_string(uint8_t value);

void covertToDimmString(uint8_t cpu, uint8_t channel, uint8_t slot,
                             std::string& str);

void getCommonDimmLocation(const _dimm_info& dimmInfo,
                                  std::string& dimmlocation,
                                  std::string& dimm);

int processOemMetaEvent(
    tid_t tid, const uint8_t* eventData, size_t eventDataSize,
    const std::map<std::string, MctpEndpoint>& configurations);

} // namespace oem_meta
} // namespace platform_mc
} // namespace pldm
