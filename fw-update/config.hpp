#include "common/types.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace pldm::fw_update
{

/** @brief Parse the firmware update config file
 *
 *  Parses the config file to generate D-Bus device inventory and firmware
 *  inventory from firmware update inventory commands. The config file also
 *  generate args for update message registry entries.
 *
 *  @param[in] jsonPath - Path of firmware update config file
 *  @param[out] deviceInventoryInfo - D-Bus device inventory config info
 *  @param[out] fwInventoryInfo - D-Bus firmware inventory config info
 *  @param[out] componentNameMapInfo - Component name info
 *
 */
void parseConfig(const fs::path& jsonPath,
                 DeviceInventoryInfo& deviceInventoryInfo,
                 FirmwareInventoryInfo& fwInventoryInfo,
                 ComponentNameMapInfo& componentNameMapInfo);

} // namespace pldm::fw_update
