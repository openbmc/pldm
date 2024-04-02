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

} // namespace deserialize
} // namespace pldm
