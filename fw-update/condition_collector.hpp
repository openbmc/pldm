#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <format>
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

                    // The schema requires a non-empty unit name and omission of
                    // the property to skip a condition. Warn on a present but
                    // empty name so a misconfiguration is not silently treated
                    // as an intentionally omitted hook.
                    if (prePath.empty() && cc.preUpdateTarget)
                    {
                        warning(
                            "Empty 'PreUpdateTarget' for component {COMPONENT}, treating it as no pre-condition",
                            "COMPONENT", cc.component);
                    }
                    if (postPath.empty() && cc.postUpdateTarget)
                    {
                        warning(
                            "Empty 'PostUpdateTarget' for component {COMPONENT}, treating it as no post-condition",
                            "COMPONENT", cc.component);
                    }

                    // Skip entries where both pre and post conditions are empty
                    if (!prePath.empty() || !postPath.empty())
                    {
                        conditionMap.insert_or_assign(
                            cc.component, ConditionPaths{prePath, postPath});
                        info(
                            "Update condition for component {COMPONENT}: pre={PRE}, post={POST}",
                            "COMPONENT", cc.component, "PRE", prePath, "POST",
                            postPath);
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

        info("Loaded update conditions of {COUNT} components from {JSPATH}",
             "COUNT", conditionMap.size(), "JSPATH", jsonPath);
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

/**
 * @brief Generate the static named arguments for a condition service
 *
 * All supported named arguments are passed to every parameterized condition
 * service, so no per-component selection is needed. Only the arguments known
 * ahead of an update are generated here; dynamic fields such as applyTime are
 * appended once startUpdate provides them.
 *
 * @param[in] boardName - The board name value to use for boardName argument
 * @return Generated argument string, empty when no value is available
 */
inline std::string generateArg(const std::string& boardName)
{
    if (boardName.empty())
    {
        return "";
    }

    return std::format("boardName={}", boardName);
}

} // namespace pldm::fw_update
