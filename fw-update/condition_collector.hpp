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
    std::optional<ConditionTargetArgs> targetArgs;
};

// Custom from_json for ComponentCondition
inline void from_json(const Json& j, ComponentCondition& cc)
{
    if (!j.contains("Component") || !j["Component"].is_string() ||
        j["Component"].get<std::string>().empty())
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

    if (j.contains("TargetArgs") && j["TargetArgs"].is_array())
    {
        cc.targetArgs = j["TargetArgs"].get<ConditionTargetArgs>();
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
                    ConditionPath prePath = cc.preUpdateTarget.value_or("");
                    ConditionPath postPath = cc.postUpdateTarget.value_or("");
                    ConditionTargetArgs targetArgs =
                        cc.targetArgs.value_or(ConditionTargetArgs{});

                    // Skip entries where both pre and post conditions are empty
                    if (!prePath.empty() || !postPath.empty())
                    {
                        ConditionInfo info{prePath, postPath, targetArgs};
                        conditionMap.insert_or_assign(cc.component, info);
                    }
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
            return std::get<0>(conditionMap.at(name));
        }
        return "";
    }

    ConditionPath postCondition(const ConditionIdentifier& name) const
    {
        if (conditionMap.contains(name))
        {
            return std::get<1>(conditionMap.at(name));
        }
        return "";
    }

    ConditionPaths conditions(const ConditionIdentifier& name) const
    {
        if (conditionMap.contains(name))
        {
            const auto& info = conditionMap.at(name);
            return ConditionPaths{std::get<0>(info), std::get<1>(info)};
        }
        return {};
    }

    ConditionTargetArgs targetArgs(const ConditionIdentifier& name) const
    {
        if (conditionMap.contains(name))
        {
            return std::get<2>(conditionMap.at(name));
        }
        return {};
    }

    ConditionInfo conditionInfo(const ConditionIdentifier& name) const
    {
        if (conditionMap.contains(name))
        {
            return conditionMap.at(name);
        }
        return {std::string{}, std::string{}, ConditionTargetArgs{}};
    }

  private:
    PrePostConditionMap conditionMap;
};

/**
 * @brief Generate static systemd interface arguments from TargetArgs and
 * provided values
 * @param[in] targetArgs - The template specifying which arguments to include
 * @param[in] boardName - The board name value to use for boardName argument
 * @return Generated argument string (comma-separated values, empty if template
 * is empty). Dynamic fields such as applyTime are appended after startUpdate.
 */
inline std::string generateArg(const ConditionTargetArgs& targetArgs,
                               const std::string& boardName)
{
    if (targetArgs.empty() || boardName.empty())
    {
        return "";
    }

    std::vector<std::string> argValues;
    for (const auto& arg : targetArgs)
    {
        if (arg == "boardName")
        {
            argValues.push_back("boardName=" + boardName);
        }
    }

    std::string result;
    for (size_t i = 0; i < argValues.size(); ++i)
    {
        if (i > 0)
        {
            result += ",";
        }
        result += argValues[i];
    }
    return result;
}

} // namespace pldm::fw_update
