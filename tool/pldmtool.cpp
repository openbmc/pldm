#include "handler.hpp"

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{

    CLI::App app{"PLDM requester tool for OpenBMC"};

    // TODO: To enable it later
    // bool verbose_flag = false;
    // app.add_flag("-v, --verbose", verbose_flag, "Output debug logs ");
    std::vector<std::string> rawCmd{};
    app.add_option("-r, --raw", rawCmd,
                   "Send a RAW PLDM request and print response");

    auto base = app.add_subcommand("BASE", "PLDM Command Type = BASE");
    std::vector<std::string> args{};
    base->add_option("-c, --command", args,
                     "PLDM request command \n"
                     "[GetPLDMTypes] Get PLDM Type \n"
                     "[GetPLDMVersion] Get PLDM Version");

    auto bios = app.add_subcommand("BIOS", "PLDM Command Type = BIOS");
    bios->add_option("-c, --command", args, "PLDM request command");

    auto oem = app.add_subcommand("OEM", "PLDM Command Type = OEM");
    oem->add_option("-c, --command", args, "PLDM request command");

    CLI11_PARSE(app, argc, argv);

    std::string cmdName;
    int rc = 0;

    if (argc < 2)
    {
        std::cout << "Run pldmtool --help for more information" << std::endl;
        rc = -1;
        return rc;
    }

    if (memcmp(argv[1], "--raw", strlen(argv[1])) != 0 &&
        memcmp(argv[1], "-r", strlen(argv[1])) != 0)
    {
        // Parse args to program
        if (args.size() == 0)
        {
            std::cout << "Run pldmtool --help for more information"
                      << std::endl;
            rc = -1;
            return rc;
        }
        cmdName = args[0];
    }
    else
    {
        if (rawCmd.size() == 0)
        {
            std::cout << "Run pldmtool --help for more information"
                      << std::endl;
            rc = -1;
            return rc;
        }

        // loop through the remaining argument list
        for (auto&& item : rawCmd)
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
        args = rawCmd;
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
