#include "config.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace pldm::fw_update
{

using Json = nlohmann::json;

void parseConfig(const fs::path& jsonFilePath,
                 DeviceInventoryInfo& deviceInventoryInfo,
                 ComponentNameMapUUID& componentNameMapUUID)
{

    if (!fs::exists(jsonFilePath))
    {
        std::cerr << "fw_update config file not found, FILE=" << jsonFilePath;
        return;
    }

    std::ifstream jsonFile(jsonFilePath);
    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        std::cerr << "Parsing fw_update config file failed, FILE="
                  << jsonFilePath;
        return;
    }

    const Json emptyJson{};

    try
    {
        auto entries = data.value("entries", emptyJson);
        for (const auto& entry : entries.items())
        {
            auto mctp_endpoint_uuid =
                entry.value()["mctp_endpoint_uuid"].get<std::string>();

            if (entry.value().contains("device_inventory"))
            {
                deviceInventoryInfo[mctp_endpoint_uuid] =
                    entry.value()["device_inventory"]["object_path"]
                        .get<std::string>();
            }

            if (entry.value().contains("component_info"))
            {
                ComponentIdNameMap componentIdNameMap{};
                for (auto& [componentName, componentID] :
                     entry.value()["component_info"].items())
                {
                    componentIdNameMap[componentID] = componentName;
                }
                componentNameMapUUID[mctp_endpoint_uuid] = componentIdNameMap;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

} // namespace pldm::fw_update