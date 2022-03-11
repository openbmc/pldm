#include "common/types.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace pldm::fw_update
{

/** @brief Parse the firmware update config file
 *
 *  @param[in] jsonPath - the path of firmware update config file
 *  @param[out] deviceInventoryInfo - device inventory config information
 *
 */
void parseConfig(const fs::path& jsonPath,
                 DeviceInventoryInfo& deviceInventoryInfo,
                 ComponentNameMapUUID& componentNameMapUUID);

} // namespace pldm::fw_update
