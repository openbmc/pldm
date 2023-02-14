#pragma once

#include <CLI/CLI.hpp>

namespace pldmtool
{
namespace platform
{
void registerCommand(CLI::App& app);

void parseGetPDROption();

void getPDRs();

} // namespace platform

} // namespace pldmtool
