#include "pldmtool.hpp"

#include "pt_base_cmd.hpp"
#include "pt_log.hpp"

#include <stdio.h>
#include <stdlib.h>

#include "libpldm/base.h"

#define VERSION "1.0"

/* structure containing command list */
/* { <function pointer>, command, "Description"} */
struct pldm_cmd pldmtool_cmd_list[] = {
    {pldm_get_tid, "GetTID", "Retrieve the Terminus ID (TID)"},
    {pldm_get_version, "GetPLDMVersion", "Retrieve the supported versions"},
    {pldm_get_types_req, "GetPLDMTypes", "Retrieve the supported types"},
    {pldm_get_commands, "GetPLDMCommands", "Retrieve the supported commands"},
    {NULL},
};

/* structure containing interface list */
/* {"Interface Name", Supported ?, "Description"} */
struct pldm_intf pldmtool_intf_list[] = {
    {"SELF", 1, "Loopback using pldm responder library "},
    {"SERIAL", 1, "Send the Message to MCTP via serial"},
    {"DBUS", 0, "Send the Message on the DBUS "},
    {NULL},
};

/*
 * Print the command list details
 */
void pldmtool_cmd_print(struct pldm_cmd* cmdlist)
{
    struct pldm_cmd* cmd;
    int hdr = 0;

    if (cmdlist == NULL)
        return;
    for (cmd = cmdlist; cmd->func != NULL; cmd++)
    {
        if (cmd->desc == NULL)
            continue;
        if (hdr == 0)
        {
            lprintf(LOG_NOTICE, "Commands:");
            hdr = 1;
        }
        lprintf(LOG_NOTICE, "\t%-12s  %s", cmd->name, cmd->desc);
    }
    lprintf(LOG_NOTICE, "");
}

/*
 * Print list of interfaces to know what is supported and what is not
 */
void pldmtool_intf_print(struct pldm_intf* intflist)
{
    struct pldm_intf* sup;
    int hdr = 0;

    if (intflist == NULL)
        return;
    for (sup = intflist; sup->name != NULL; sup++)
    {
        if (sup->desc == NULL)
            continue;
        if (hdr == 0)
        {
            lprintf(LOG_NOTICE, "Interfaces:");
            hdr = 1;
        }
        lprintf(LOG_NOTICE, "\t%-12s  %s Supported %s", sup->name, sup->desc,
                sup->supported ? "[YES]" : "[NO]");
    }
    lprintf(LOG_NOTICE, "");
}

/*
 * Main program
 */
int main(int argc, char** argv)
{
    int rc = 0;

    printf("\n\nStarting the PLDMTOOL \n");
    // rc = pldmtool_main(argc, argv, pldmtool_cmd_list, pldmtool_intf_list);
    rc = pldmtool_main(argc, argv);
    printf("\nCompleted Running PLDMTOOL \n\n");

    log_halt();

    if (rc < 0)
    {
        printf("Failure encountered \n");
        exit(EXIT_FAILURE);
    }
    else
    {
        exit(EXIT_SUCCESS);
    }
}

/*
 * To display help on screen
 */
void pldmtool_option_usage(const char* progname)
{
    lprintf(LOG_NOTICE, "\n %s version %s\n", progname, VERSION);
    lprintf(LOG_NOTICE, "usage: %s [options...] <command>\n", progname);
    lprintf(LOG_NOTICE, "Options:");
    lprintf(LOG_NOTICE, "    -h             This help");
    lprintf(LOG_NOTICE, "    -V             Show PLDMTool version information");
    lprintf(LOG_NOTICE, "    -v             Verbose [debug information]	");
    lprintf(LOG_NOTICE,
            "    -t             4 byte hex Version of the PLDM version");
    lprintf(LOG_NOTICE, "    -I Interfaces  Interface to use");
    lprintf(LOG_NOTICE, "");
    pldmtool_intf_print(pldmtool_intf_list);
    pldmtool_cmd_print(pldmtool_cmd_list);
}

/* Function to handle parsing and executing command line options
 *
 * @argc:	count of options
 * @argv:	list of options
 * @cmdlist:	list of supported commands
 * @intflist:	list of supported interfaces
 *
 * returns 0 on success
 * returns -1 on error
 */
int pldmtool_main(int argc, char** argv)
{
    char* progname = NULL;
    int argflag = -1;
    int found = 0;
    char* intfname = NULL;
    struct pldm_intf* sup;
    int rc = -1;
    int failure = 0;
    struct pldm_cmd* cmd;
    struct pldm_intf* intflist = pldmtool_intf_list;
    struct pldm_cmd* cmdlist = pldmtool_cmd_list;
    int command_completed = 0;

    /* save program name */
    progname = strrchr(argv[0], '/');
    progname = ((progname == NULL) ? argv[0] : progname + 1);
    log_init("pldmtool", CONSOLELOG, 0);
    // printf("program Name [%s]\n",progname);
    while ((argflag = getopt(argc, (char**)argv, OPTION_STRING)) != -1)
    {
        switch (argflag)
        {
            case 'I':
                if (intfname)
                {
                    free(intfname);
                    intfname = NULL;
                }
                intfname = strdup(optarg);
                if (intfname == NULL)
                {
                    lprintf(LOG_ERR, "%s: malloc failure", progname);
                    failure = 1;
                    break;
                }
                if (intflist != NULL)
                {
                    found = 0;
                    for (sup = intflist; sup->name != NULL; sup++)
                    {
                        if (strncmp(sup->name, intfname, strlen(intfname)) ==
                                0 &&
                            strncmp(sup->name, intfname, strlen(sup->name)) ==
                                0 &&
                            sup->supported == 1)
                            found = 1;
                    }
                    if (!found)
                    {
                        lprintf(LOG_ERR, "Interface %s not supported",
                                intfname);
                        failure = 1;
                        break;
                    }
                }
                break;
            case 'h':
                pldmtool_option_usage(progname);
                command_completed = 1;
                break;
            case 'V':
                lprintf(LOG_NOTICE, "%s version %s\n", progname, VERSION);
                command_completed = 1;
                break;
            case 'v':
                log_level_set(LOG_DEBUG);
                break;
            case 't':
                /* used by command functions directly so no-op here*/
                break;
            case '?':
                lprintf(LOG_NOTICE, "unknown option: %c\n", optopt);
                pldmtool_option_usage(progname);
                command_completed = 1;
                break;
            default:
                lprintf(LOG_NOTICE, "Reached default [%c]", argflag);
                pldmtool_option_usage(progname);
                command_completed = 1;
                break;
        }
        if (1 == command_completed)
        {
            break;
        }
    }

    do
    {
        if (1 == command_completed)
        {
            rc = 0;
            break;
        }
        lprintf(LOG_DEBUG, "found [%d]\n", found);
        if (0 == found)
        {
            lprintf(LOG_NOTICE, "Interface option not passed/supported so "
                                "defaulting to loopback");
            lprintf(LOG_DEBUG, "Setting -I as SELF ");
            intfname = strdup("SELF");
        }
        /* check for command before doing anything */
        lprintf(LOG_DEBUG, "argc-optind [%d]\n", argc - optind);
        if (argc - optind < 1)
        {
            lprintf(LOG_ERR, "No command provided!");
            pldmtool_option_usage(progname);
            rc = -1;
            break;
        }
        if (argc - optind > 0)
        {
            lprintf(LOG_DEBUG, "command found [%s]", argv[optind]);
            if (strncmp(argv[optind], "help", 4) == 0)
            {
                pldmtool_cmd_print(cmdlist);
                rc = 0;
                break;
            }
            for (cmd = cmdlist; cmd->func != NULL; cmd++)
            {
                if (strncmp(argv[optind], cmd->name,
                            __maxlen(cmd->name, argv[optind])) == 0)
                    break;
            }
            if (cmd->func == NULL)
            {
                lprintf(LOG_ERR, "Invalid command: %s", argv[optind]);
                rc = -1;
                break;
            }
            /* Call the function to be executed*/
            rc = cmd->func(intfname, argc, argv);
            break;
        }
    } while (false);

    if (failure)
    {
        lprintf(LOG_ERR, "failure encountered: [%d]", failure);
        rc = -1;
    }
    if (intfname != NULL)
    {
        free(intfname);
        intfname = NULL;
    }
    lprintf(LOG_DEBUG, "pldmtool_main exiting::  rc [%d]", rc);
    return rc;
}
