#include "handler.hpp"

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{

    CLI::App app{"PLDM requester tool for OpenBMC"};

    // TODO: To enable it later
    // bool verbose_flag = false;
    // app.add_flag("-v, --verbose", verbose_flag, "Output debug logs ");

    std::string subCmdName;
    auto subCmd =
        app.add_option_group("SUB COMMAND", "Specify sub command to use");
    CLI::Option* opt = subCmd->add_option("BASE | BIOS | OEM", subCmdName,
                                          "PLDM Command Type");

    std::string pldmCmdName;
    auto pldmCmd = app.add_option_group("PLDM request command",
                                        "Specify PLDM command to test");
    pldmCmd->add_option("GetPLDMTypes", pldmCmdName, "Get Type");
    pldmCmd->add_option("GetPLDMVersion", pldmCmdName, "Get Version")
        ->needs(opt);

    std::vector<std::string> args{};
    app.add_option("-c, —command", args, "PLDM request command")->required();

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
