
#ifndef PT_BASE_CMD_H
#define PT_BASE_CMD_H

/** @brief Handle GetPLDMTypes command
 *
 *  @param[in] intfname - The interface to be used to send request
 *  @param[in] argc - An integer argument count of the command line arguments
 *  @param[in] argv - An argument list of the command line arguments
 *
 *  @return - 0 on success, -1 on error
 */
int pldm_get_types_req(char* intfname, int argc, char** argv);

/** @brief Handle GetPLDMCommands command
 *
 *  @param[in] intfname - The interface to be used to send request
 *  @param[in] argc - An integer argument count of the command line arguments
 *  @param[in] argv - An argument list of the command line arguments
 *
 *  @return - 0 on success, -1 on error
 */
int pldm_get_commands(char* intfname, int argc, char** argv);

/** @brief Handle GetTID command
 *
 *  @param[in] intfname - The interface to be used to send request
 *  @param[in] argc - An integer argument count of the command line arguments
 *  @param[in] argv - An argument list of the command line arguments
 *
 *  @return - 0 on success, -1 on error
 */
int pldm_get_tid(char* intfname, int argc, char** argv);

/** @brief Handle GetPLDMVersion command
 *
 *  @param[in] intfname - The interface to be used to send request
 *  @param[in] argc - An integer argument count of the command line arguments
 *  @param[in] argv - An argument list of the command line arguments
 *
 *  @return - 0 on success, -1 on error
 */
int pldm_get_version(char* intfname, int argc, char** argv);

#endif /* PT_BASE_CMD_H */
