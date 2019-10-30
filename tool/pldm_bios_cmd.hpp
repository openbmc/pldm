#pragma once

#ifndef PLDM_BIOS_CMD_H
#define PLDM_BIOS_CMD_H

#include "pldm_cmd_helper.hpp"

/** @brief Handler for GetBIOSTable command
 *  *
 *   *  @param[in]  args - Argument to be passed to the handler.
 *    *                     Optional argument.
 *     *
 *      *  @return - None
 *       */
void getBIOSTable(std::vector<std::string>&& args);

/** @brief Handler for GetDateTime command
 *  *
 *   *  @param[in]  args - Argument to be passed to the handler.
 *    *                     Optional argument.
 *     *
 *      *  @return - None
 *       */
void getDateTime(std::vector<std::string>&& args);

#endif /* PLDM_BIOS_CMD_H */
