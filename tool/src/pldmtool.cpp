#include "handler.hpp"

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{

    CLI::App app{"PLDM requester tool for OpenBMC"};

    // TODO: To enable it later
    // bool verbose_flag = false;
    // app.add_flag("-v, --verbose", verbose_flag, "Output debug logs ");
    std::vector<std::string> args{};
    app.add_option("-c, —command", args, "PLDM request command")->required();

    std::string pldmCmdName;
    app.add_option("GetPLDMTypes", pldmCmdName, "Get PLDM Type");
    app.add_option("GetPLDMVersion", pldmCmdName, "Get PLDM Version");

    app.add_subcommand("BASE", "PLDM Command Type = BASE");
    app.add_subcommand("BIOS", "PLDM Command Type = BIOS");
    app.add_subcommand("OEM", "PLDM Command Type = OEM");

    CLI11_PARSE(app, argc, argv);

    Handler handler;

    // Parse args to program
    std::string cmdName = args[0];
    int rc = 0;

    try
    {
        handler.dispatcher.at(cmdName)(std::move(args));
    }
    catch (const std::out_of_range& e)
    {
        std::cerr << cmdName << " is not supported!" << std::endl;
        rc = -1;
    }
    return (rc < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}
