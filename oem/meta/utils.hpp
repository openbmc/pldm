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
    uint64_t EndpointId;
    uint64_t bus;
    std::string name;
    std::optional<std::string> iana;
};

bool checkMetaIana(
    pldm_tid_t tid,
    const std::map<pldm::dbus::ObjectPath, MctpEndpoint>& configurations);

std::string getSlotNumberByTID(
    const std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>&
        configurations,
    pldm_tid_t tid);

} // namespace oem_meta
} // namespace pldm
