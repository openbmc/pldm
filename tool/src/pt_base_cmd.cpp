
#include "pt_base_cmd.hpp"

#include "libpldmresponder/base.hpp"
#include "pldmtool.hpp"
#include "pt_log.hpp"

#include <stdint.h>
#include <stdlib.h>

#include "libpldm/base.h"

/*
 * Handles the GetPLDMTypes command
 */
int pldm_get_types_req(char* intfname, int argc, char** argv)
{
    int rc = -1;
    uint8_t instance_id = 27;
    struct pldm_msg* req_msg = NULL;
    struct pldm_msg* resp_msg = NULL;
    // struct pldm_msg_payload* resp_request =NULL; //temp
    int argflag = -1;
    bool detailed_output = false;

    lprintf(LOG_DEBUG, "pldm_get_types_req entered with interface [%s]",
            intfname);

    while ((argflag = getopt(argc, (char**)argv, OPTION_STRING)) != -1)
    {
        switch (argflag)
        {
            case 'd':
                detailed_output = true;
                break;
            default:
                break;
        }
    }
    do
    {
        req_msg = (struct pldm_msg*)malloc(sizeof(struct pldm_msg));
        rc = encode_get_types_req(instance_id, req_msg);
        if (rc > 0)
        {
            rc = -1;
            break;
        }
        lprintf(LOG_DEBUG, "GetPLDMTypes ecoded request successfully");

        if (true == detailed_output)
        {
            lprintf(LOG_DEBUG, "GetPLDMTypes detailed request message asked");
            lprintf(LOG_DEBUG, "To be supported");
        }
        else
        {
            lprintf(LOG_DEBUG, "GetPLDMTypes detailed request not needed");
        }

        // Send the request
        if (strncmp("SELF", intfname, strlen(intfname)) == 0)
        {
            lprintf(LOG_DEBUG, "GetPLDMTypes direct libresponder usage asked");
            resp_msg = (struct pldm_msg*)malloc(sizeof(struct pldm_msg));
            uint8_t resp_payload[8] = {0};
            resp_msg->body.payload = resp_payload;
            resp_msg->body.payload_length = sizeof(resp_payload);
            lprintf(LOG_DEBUG,
                    "GetPLDMTypes sending request message to libresponder ");
            pldm::responder::getPLDMTypes(NULL, resp_msg);
            lprintf(LOG_DEBUG,
                    "GetPLDMTypes received libresponder response message");
            lprintf(LOG_DEBUG,
                    "GetPLDMTypes Printing libresponder response message");
            lprintf(LOG_DEBUG, "GetPLDMTypes Response Byte 1 [%d]",
                    resp_payload[0]);
            lprintf(LOG_DEBUG, "GetPLDMTypes Response Byte 2 [%d]",
                    resp_payload[1]);
            lprintf(LOG_DEBUG, "GetPLDMTypes Response Byte 3 [%d]",
                    resp_payload[2]);
            lprintf(LOG_DEBUG, "GetPLDMTypes Response Byte 4 [%d]",
                    resp_payload[3]);
            lprintf(LOG_DEBUG, "GetPLDMTypes Response Byte 5 [%d]",
                    resp_payload[4]);
            lprintf(LOG_DEBUG, "GetPLDMTypes Response Byte 6 [%d]",
                    resp_payload[5]);
            lprintf(LOG_DEBUG, "GetPLDMTypes Response Byte 7 [%d]",
                    resp_payload[6]);
            lprintf(LOG_DEBUG, "GetPLDMTypes Response Byte 8 [%d]",
                    resp_payload[7]);
            lprintf(LOG_DEBUG, "GetPLDMTypes Response payload length [%x]",
                    resp_msg->body.payload_length);
        }
        else if (strncmp("DBUS", intfname, strlen(intfname)) == 0)
        {
            lprintf(LOG_DEBUG, "GetPLDMTypes DBUS support to be added");
            lprintf(LOG_DEBUG, "GetPLDMTypes DBUS currently unsupported");
        }
        else
        {
            lprintf(LOG_ERR, "Unknown Interface");
            rc = -1;
            break;
        }
    } while (false);

    if (req_msg)
    {
        free(req_msg);
    }
    if (resp_msg)
    {
        free(resp_msg);
    }

    lprintf(LOG_DEBUG, "pldm_get_types_req entered with rc [%d]", rc);
    return rc;
}

/*
 * Handles the GetPLDMCommands command
 */
int pldm_get_commands(char* intfname, int argc, char** argv)
{
    int rc = -1;
    /* TBD */
    rc = 0;
    return rc;
}

/*
 * Handles the GetTID command
 */
int pldm_get_tid(char* intfname, int argc, char** argv)
{
    int rc = -1;
    /* TBD */
    rc = 0;
    return rc;
}

/*
 * Handles the GetPLDMVersion command
 */
int pldm_get_version(char* intfname, int argc, char** argv)
{
    int rc = -1;
    /* TBD */
    rc = 0;
    return rc;
}
