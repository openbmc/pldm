#include "config.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace pldm::fw_update
{

using Json = nlohmann::json;

void parseConfig(const fs::path& jsonPath,
                 DeviceInventoryInfo& deviceInventoryInfo,
                 FirmwareInventoryInfo& fwInventoryInfo,
                 ComponentNameMapInfo& componentNameMapInfo)
{
    if (!fs::exists(jsonPath))
    {
        // No error tracing to avoid polluting journal for users not using
        // config JSON
        return;
    }

    std::ifstream jsonFile(jsonPath);
    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        std::cerr << "Parsing fw_update config file failed, FILE=" << jsonPath
                  << "\n";
        return;
    }

    const Json emptyJson{};

    auto entries = data.value("entries", emptyJson);
    for (const auto& entry : entries.items())
    {
        auto mctp_endpoint_uuid =
            entry.value()["mctp_endpoint_uuid"].get<std::string>();

        if (entry.value().contains("device_inventory"))
        {
            auto objPath = entry.value()["device_inventory"]["object_path"]
                               .get<std::string>();

            Associations assocs{};
            if (entry.value()["device_inventory"].contains("associations"))
            {
                auto associations =
                    entry.value()["device_inventory"]["associations"];
                for (const auto& assocEntry : associations.items())
                {
                    auto forward = assocEntry.value()["forward"];
                    auto reverse = assocEntry.value()["reverse"];
                    auto endpoint = assocEntry.value()["endpoint"];
                    assocs.emplace_back(
                        std::make_tuple(forward, reverse, endpoint));
                }
            }

            deviceInventoryInfo[mctp_endpoint_uuid] =
                std::make_tuple(std::move(objPath), std::move(assocs));
        }

        if (entry.value().contains("firmware_inventory"))
        {
            ComponentIdNameMap componentIdNameMap{};
            for (auto& [componentName, componentID] :
                 entry.value()["firmware_inventory"].items())
            {
                componentIdNameMap[componentID] = componentName;
            }
            if (componentIdNameMap.size())
            {
                fwInventoryInfo[mctp_endpoint_uuid] = componentIdNameMap;
            }
        }

        if (entry.value().contains("component_info"))
        {
            ComponentIdNameMap componentIdNameMap{};
            for (auto& [componentName, componentID] :
                 entry.value()["component_info"].items())
            {
                componentIdNameMap[componentID] = componentName;
            }
            if (componentIdNameMap.size())
            {
                componentNameMapInfo[mctp_endpoint_uuid] = componentIdNameMap;
            }
        }
    }
}

} // namespace pldm::fw_update