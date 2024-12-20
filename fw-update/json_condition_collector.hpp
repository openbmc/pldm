#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>

namespace pldm
{

namespace fw_update
{

using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm;
namespace fs = std::filesystem;
using Json = nlohmann::json;

class JsonConditionCollector : public FirmwareTypeConditionMap
{
  public:
    explicit JsonConditionCollector(fs::path jsonPath)
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
            if (data.contains("Components") && data["Components"].is_array())
            {
                for (const auto& component : data["Components"])
                {
                    if (component.contains("Component") &&
                        component["Component"].is_string())
                    {
                        auto [it, inserted] =
                            insert_or_assign(component["Component"],
                                             FirmwareConditionPathPair());
                        auto& [pre, post] = it->second;
                        if (component.contains("PreSetupTarget") &&
                            component["PreSetupTarget"].is_string())
                        {
                            pre = component["PreSetupTarget"];
                        }
                        if (component.contains("PostSetupTarget") &&
                            component["PostSetupTarget"].is_string())
                        {
                            post = component["PostSetupTarget"];
                        }
                    }
                    else
                    {
                        error("A bad Component Attributes for {JSPATH} found",
                              "JSPATH", jsonPath);
                        continue;
                    }
                }
            }
            else
            {
                error("Bad Json format for parsing: {JSPATH}", "JSPATH",
                      jsonPath);
            }
        }
        catch (const Json::parse_error& e)
        {
            error("Failed to parse the json file: {JSPATH}, error: {ERROR}",
                  "JSPATH", jsonPath, "ERROR", e.what());
            return;
        }
    }

    FirmwareConditionPath preCondition(const FirmwareDeviceName& name) const
    {
        if (contains(name))
        {
            return at(name).first;
        }
        return "";
    }

    FirmwareConditionPath postCondition(const FirmwareDeviceName& name) const
    {
        if (contains(name))
        {
            return at(name).second;
        }
        return "";
    }

    FirmwareConditionPathPair conditions(const FirmwareDeviceName& name) const
    {
        if (contains(name))
        {
            return at(name);
        }
        return {};
    }
};

} // namespace fw_update
} // namespace pldm
