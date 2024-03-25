#pragma once

#include "../common/utils.hpp"
#include "host-bmc/host_pdr_handler.hpp"

#include <filesystem>
#include <fstream>

namespace pldm
{
namespace deserialize
{

/** @brief Restoring Dbus values from persistent file.
 *
 */
void restoreDbusObj(HostPDRHandler* hostPDRHandler);

std::pair<std::set<uint16_t>, std::set<uint16_t>>
    getEntityTypes(const fs::path& path);

} // namespace deserialize
} // namespace pldm
