#include "pldmtool.hpp"

using namespace std;

int main(int argc, char** argv)
{

    int rc = 0;
    std::string pldm_cmd;
    std::set<std::string> pldm_cmd_items = {"GetTID", "GetPLDMVersion",
                                            "GetPLDMTypes", "GetPLDMCommands"};
    bool verbose_flag;

    CLI::App app{"OpenBMC PLDMTOOL"};
    app.add_set("-c, --command", pldm_cmd, pldm_cmd_items,
                "PLDM command to test", true)
        ->required()
        ->group("Standard");
    app.add_flag("-v, --verbose", verbose_flag, "Verbose [debug information] ");
    CLI11_PARSE(app, argc, argv);

    std::string action{argv[2]};

    const CallbackMap& callbacks = Registration::getCallbacks();

    auto callback = callbacks.find(action);

    if (callback == callbacks.end())
    {
        rc = -1;
        return rc;
    }

    callback->second();

    return (rc < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}
