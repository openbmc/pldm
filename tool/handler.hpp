#pragma once

#include "pldm_base_cmd.hpp"
#include "pldm_bios_cmd.hpp"

#include <functional>
#include <map>
#include <string>
#include <typeindex>

using Cmd = std::string;
using Args = std::vector<std::string>;
class Handler
{
  public:
    const std::map<Cmd, std::function<void(Args&&)>> dispatcher{
        {"GetPLDMTypes",
         [](Args&& args) { return getPLDMTypes(std::move(args)); }},
        {"GetPLDMVersion",
         [](Args&& args) { return getPLDMVersion(std::move(args)); }},
        {"HandleRawOp",
         [](Args&& args) { return handleRawOp(std::move(args)); }},
        {"GetBIOSTable",
         [](Args&& args) { return getBIOSTable(std::move(args)); }},
        {"GetDateTime",
         [](Args&& args) { return getDateTime(std::move(args)); }}};
};
