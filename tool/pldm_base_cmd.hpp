#pragma once

#ifndef PLDM_BASE_CMD_H
#define PLDM_BASE_CMD_H

#include "pldm_cmd_helper.hpp"

/** @brief Handler for GetPLDMTypes command
 *
 *  @param[in]  args - Argument to be passed to the handler.
 *                     Optional argument.
 *
 *  @return - None
 */
void getPLDMTypes(std::vector<std::string>&& args);

/** @brief Handler for GetPLDMVersion command
 *
 *
 *  @param[in]  args - Argument to be passed to the handler
 *              e.g :  PLDM Command Type : base, bios etc.
 *
 *  @return - None
 */
void getPLDMVersion(std::vector<std::string>&& args);

/** @brief Handler for Raw PLDM commands
 *
 *
 *  @param[in]  args - Argument to be passed to the handler
 *              e.g :  PLDM Command Type : base, bios etc.
 *
 *  @return - None
 */
void handleRawOp(std::vector<std::string>&& args);

#endif /* PLDM_BASE_CMD_H */
