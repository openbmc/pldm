#include "pldm_base_cmd.hpp"

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{

    CLI::App app{"PLDM requester tool for OpenBMC"};

    registerBaseCommand(app);

    CLI11_PARSE(app, argc, argv);
}
