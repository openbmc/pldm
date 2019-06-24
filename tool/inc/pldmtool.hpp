
#ifndef PLDMTOOL_H
#define PLDMTOOL_H

#include "logger.hpp"
#include "registration.hpp"

#include <CLI/CLI.hpp>

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
