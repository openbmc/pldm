#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <fstream>
#include <optional>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

namespace fs = std::filesystem;
using Json = nlohmann::json;

// Helper structure for JSON deserialization
struct ComponentCondition
{
    std::string component;
    std::optional<std::string> preUpdateTarget;
    std::optional<std::string> postUpdateTarget;
};

// Custom from_json for ComponentCondition
inline void from_json(const Json& j, ComponentCondition& cc)
{
    if (!j.contains("Component") || !j["Component"].is_string())
    {
        throw std::runtime_error("Missing or invalid 'Component' field");
    }
    cc.component = j["Component"].get<std::string>();

    if (j.contains("PreUpdateTarget") && j["PreUpdateTarget"].is_string())
    {
        cc.preUpdateTarget = j["PreUpdateTarget"].get<std::string>();
    }

    if (j.contains("PostUpdateTarget") && j["PostUpdateTarget"].is_string())
    {
        cc.postUpdateTarget = j["PostUpdateTarget"].get<std::string>();
    }
}

class ConditionConfigManager
{
  public:
    explicit ConditionConfigManager(fs::path jsonPath)
    {
        if (jsonPath.empty())
        {
            return;
        }
        if (!fs::exists(jsonPath))
        {
            error("Json file does not exist: {JSPATH}", "JSPATH", jsonPath);
            return;
        }
        std::ifstream jsonFile(jsonPath);
        if (!jsonFile.is_open())
        {
            error("Failed to open the json file: {JSPATH}", "JSPATH", jsonPath);
            return;
        }
        try
        {
            Json data = Json::parse(jsonFile);
            if (!data.contains("Components") || !data["Components"].is_array())
            {
                error("Bad Json format for parsing: {JSPATH}", "JSPATH",
                      jsonPath);
                return;
            }

            for (const auto& componentJson : data["Components"])
            {
                try
                {
                    ComponentCondition cc =
                        componentJson.get<ComponentCondition>();
                    ConditionPaths paths{cc.preUpdateTarget.value_or(""),
                                         cc.postUpdateTarget.value_or("")};
                    conditionMap.insert_or_assign(cc.component, paths);
                }
                catch (const std::exception& e)
                {
                    error("Failed to parse component: {ERROR}", "ERROR",
                          e.what());
                    continue;
                }
            }
        }
        catch (const Json::parse_error& e)
        {
            error("Failed to parse the json file: {JSPATH}, error: {ERROR}",
                  "JSPATH", jsonPath, "ERROR", e.what());
            return;
        }
    }

    ConditionPath preCondition(const ConditionIdentifier& name) const
    {
        if (conditionMap.contains(name))
        {
            return conditionMap.at(name).first;
        }
        return "";
    }

    ConditionPath postCondition(const ConditionIdentifier& name) const
    {
        if (conditionMap.contains(name))
        {
            return conditionMap.at(name).second;
        }
        return "";
    }

    ConditionPaths conditions(const ConditionIdentifier& name) const
    {
        if (conditionMap.contains(name))
        {
            return conditionMap.at(name);
        }
        return {};
    }

  private:
    PrePostConditionMap conditionMap;
};

} // namespace pldm::fw_update
