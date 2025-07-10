#pragma once

#include <libpldm/base.h>

#include <common/types.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace pldm
{
namespace oem_meta
{

struct MctpEndpoint
{
    uint64_t address;
    // uint64_t EndpointId;
    uint64_t bus;
    std::string name;
    std::optional<std::string> iana;
    pldm::dbus::ObjectPath objectPath;
};

bool checkMetaIana(const std::map<eid, MctpEndpoint>& configurations,
                   pldm_tid_t tid);

std::string getSlotNumberStringByTID(
    const std::map<eid, MctpEndpoint>& configurations, pldm_tid_t tid);

} // namespace oem_meta
} // namespace pldm
