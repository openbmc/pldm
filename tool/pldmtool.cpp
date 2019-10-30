#include "handler.hpp"

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{

    CLI::App app{"PLDM requester tool for OpenBMC"};

    // TODO: To enable it later
    // bool verbose_flag = false;
    // app.add_flag("-v, --verbose", verbose_flag, "Output debug logs ");
    std::vector<std::string> args{};
    app.add_option("-c, —command", args, "  PLDM request command");

    std::string rawCmd;
    app.add_option("raw", rawCmd, "Send a RAW PLDM request and print response");

    std::string pldmCmdName;
    app.add_option("GetPLDMTypes", pldmCmdName, "Get PLDM Type");
    app.add_option("GetPLDMVersion", pldmCmdName, "Get PLDM Version");
    app.add_option("GetBIOSTable", pldmCmdName, "Get BIOS Table");
    app.add_option("GetDateTime", pldmCmdName, "Get Date Time");

    app.add_subcommand("BASE", "PLDM Command Type = BASE");
    app.add_subcommand("BIOS", "PLDM Command Type = BIOS");
    app.add_subcommand("OEM", "PLDM Command Type = OEM");

    CLI11_PARSE(app, argc, argv);

    std::string cmdName;
    int rc = 0;

    if (args[0] != "raw")
    {
        // Parse args to program
        cmdName = args[0];
    }
    else
    {
        // removing 'raw' from the args list since it is positional argument
        // and not an input value.
        args.erase(args.begin());

        // loop through the remaining argument list
        for (auto&& item : args)
        {

            if (item[0] == '0' && (item[1] == 'x' || item[1] == 'X'))
            {

                // Erase 0x from input
                item.erase(0, 2);

                // Check for hex input value validity
                if (std::all_of(item.begin(), item.end(), ::isxdigit))
                {

                    // Parse args to program
                    cmdName = "HandleRawOp";
                }
                else
                {
                    std::cerr << item << " contains non hex digits. Re-enter"
                              << std::endl;
                    rc = -1;
                    return rc;
                }
            }
            else
            {
                std::cout << item << " Input hex value starting with 0x "
                          << std::endl;
                rc = -1;
                return rc;
            }
        }
    }

    Handler handler;
    try
    {
        handler.dispatcher.at(cmdName)(std::move(args));
    }
    catch (const std::out_of_range& e)
    {
        std::cerr << cmdName << " is not supported!" << std::endl;
        rc = -1;
    }
    return rc;
}
