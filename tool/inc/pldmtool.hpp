
#ifndef PLDMTOOL_H
#define PLDMTOOL_H

#include <string.h>
#include <unistd.h>

/* macro to find the max length from the input strings */
#define __maxlen(a, b)                                                         \
    ({                                                                         \
        int x = strlen(a);                                                     \
        int y = strlen(b);                                                     \
        (x > y) ? x : y;                                                       \
    })

/* define for supported options for getopt() function*/
#define OPTION_STRING "I:hVvdc"

/** @struct pldm_intf
 *
 * Structure representing interfaces to be used
 */
struct pldm_intf
{
    const char* name;
    int supported;
    const char* desc;
};

/** @struct pldm_cmd
 *
 * Structure representing commands to be used
 */
struct pldm_cmd
{
    int (*func)(char* intfname, int argc, char** argv);
    const char* name;
    const char* desc;
};

/** @brief Print the command list
 *
 *  @param[in] cmdlist - structure containing the command list
 *
 *  @return - None
 */
void pldmtool_cmd_print(struct pldm_cmd* cmdlist);

/** @brief Print the interface list
 *
 *  @param[in] intflist - structure containing the interface list
 *
 *  @return - None
 */
void pldmtool_intf_print(struct pldm_intf* intflist);

/** @brief Print the interface list
 *
 *  @param[in] argc - An integer argument count of the command line arguments
 *  @param[in] argv - An argument list of the command line arguments
 *
 *  @return -  successful termination status on success,
 *             an unsuccessful termination status on failure
 */
int main(int argc, char** argv);

/** @brief Usage option / help  display for the pldmtool
 *
 *  @param[in] progname - Program name to be displayed
 *
 *  @return - None
 */
void pldmtool_option_usage(const char* progname);

/** @brief Handle parsing and executing command line options
 *
 *  @param[in] argc - An integer argument count of the command line arguments
 *  @param[in] argv - An argument list of the command line arguments
 *
 *  @return - 0 on success, -1 on error
 */
int pldmtool_main(int argc, char** argv);

#endif /* PLDMTOOL_H */
