#pragma once

#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <format>
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

inline bool checkMetaIana(
    pldm_tid_t tid, const std::map<std::string, MctpEndpoint>& configurations)
{
    static constexpr std::string MetaIANA = "0015A000";

    for (const auto& [configDbusPath, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId == tid)
        {
            if (mctpEndpoint.iana.has_value() &&
                mctpEndpoint.iana.value() == MetaIANA)
            {
                return true;
            }
            else if (mctpEndpoint.iana.value() != MetaIANA)
            {
                lg2::error("WRONG IANA {IANA}", "IANA",
                           mctpEndpoint.iana.value());
                return false;
            }
        }
    }
    return false;
}

} // namespace oem_meta
} // namespace pldm
