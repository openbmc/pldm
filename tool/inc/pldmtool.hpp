
#ifndef PLDMTOOL_H
#define PLDMTOOL_H

#include <string.h>
#include <phosphor-logging/log.hpp>
#include <CLI/CLI.hpp>
#include "registration.hpp"
#include "logger.hpp"
#include "pldm_base_cmd.hpp"

#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#define VERSION "1.0"

/** @brief Main function
 *
 *  @param[in] argc - An integer argument count of the command line arguments
 *  @param[in] argv - An argument list of the command line arguments
 *
 *  @return -  successful termination status on success,
 *             an unsuccessful termination status on failure
 */
int main(int argc, char** argv);

#endif /* PLDMTOOL_H */
