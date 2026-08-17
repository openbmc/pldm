#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <unordered_map>

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
    bool stopSensorPolling = false;

    bool operator==(const ComponentCondition&) const = default;
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

    if (j.contains("StopSensorPolling") && j["StopSensorPolling"].is_boolean())
    {
        cc.stopSensorPolling = j["StopSensorPolling"].get<bool>();
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

        info("Found condition file: {JSPATH}", "JSPATH", jsonPath);

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

                    info(
                        "Parsed component {COMPONENT} condition: pre='{PRE}' post='{POST}' stopSensorPolling={STOP}",
                        "COMPONENT", cc.component, "PRE",
                        cc.preUpdateTarget.value_or(""), "POST",
                        cc.postUpdateTarget.value_or(""), "STOP",
                        cc.stopSensorPolling);

                    // The schema requires a non-empty unit name and omission of
                    // the property to skip a condition. Warn on a present but
                    // empty name so a misconfiguration is not silently treated
                    // as an intentionally omitted hook.
                    if (cc.preUpdateTarget && cc.preUpdateTarget->empty())
                    {
                        warning(
                            "Empty 'PreUpdateTarget' for component {COMPONENT}, treating it as no pre-condition",
                            "COMPONENT", cc.component);
                    }
                    if (cc.postUpdateTarget && cc.postUpdateTarget->empty())
                    {
                        warning(
                            "Empty 'PostUpdateTarget' for component {COMPONENT}, treating it as no post-condition",
                            "COMPONENT", cc.component);
                    }

                    conditionMap.insert_or_assign(cc.component, cc);
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

    /** @brief The named component's condition config
     *
     *  @param[in] name - ConditionIdentifier (component name) to look up
     *
     *  @return The component's ComponentCondition; a default-constructed
     *          ComponentCondition when the component is not configured
     */
    ComponentCondition getCondition(const ConditionIdentifier& name) const
    {
        if (conditionMap.contains(name))
        {
            return conditionMap.at(name);
        }
        return {};
    }

  private:
    std::unordered_map<ConditionIdentifier, ComponentCondition> conditionMap;
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
