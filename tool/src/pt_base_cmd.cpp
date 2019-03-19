
#include "pt_base_cmd.hpp"

#include "libpldmresponder/base.hpp"
#include "pldmtool.hpp"
#include "pt_log.hpp"

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

#include <array>
#include <bitset>

#include "libmctp/libmctp-serial.h"
#include "libmctp/libmctp.h"
#include "libpldm/base.h"

#define PLDM_COMPLETION_CODE_BYTES 1
#define PLDM_LOCAL_INSTANCE_ID 27

/*
 * Handles the GetPLDMTypes response from MTCP via SERIAL
 */
static void GetPLDMTypes_rx_Callback(uint8_t eid, void* data, void* msg,
                                     size_t len)
{
    struct pldm_msg_payload* msg_payload = NULL;
    lprintf(LOG_DEBUG,
            "Input Parameters eid [%d] data [0x%x] msg [0x%x] len [%d]", eid,
            data, msg, len);
    do
    {
        if (eid)
        {
            lprintf(LOG_ERR, " Error Received in GetPLDMTypes_rx_Callback");
            break;
        }
        if (!msg)
        {
            lprintf(LOG_ERR,
                    " Null message Received in GetPLDMTypes_rx_Callback");
            break;
        }
        if (PLDM_GET_TYPES_RESP_BYTES > len)
        {
            lprintf(
                LOG_ERR,
                " Invalid message size Received in GetPLDMTypes_rx_Callback");
            break;
        }
        lprintf(LOG_INFO, " Successfully recieved message response ");
        // Create the response message
        int decode_return = 0;
        uint8_t completion_code = -1;
        int loop_number = 0;
        msg_payload =
            (struct pldm_msg_payload*)malloc(sizeof(struct pldm_msg_payload));
        msg_payload->payload =
            static_cast<uint8_t*>(msg) + sizeof(pldm_msg_hdr);
        ;
        msg_payload->payload_length = PLDM_GET_TYPES_RESP_BYTES;
        uint8_t types[PLDM_GET_TYPES_RESP_BYTES - PLDM_COMPLETION_CODE_BYTES] =
            {0};
        for (int value : types)
        {
            lprintf(LOG_DEBUG, " Response Byte %d [%s]", loop_number++,
                    ((std::bitset<8>(value)).to_string()).c_str());
        }
        decode_return = decode_get_types_resp(
            const_cast<struct pldm_msg_payload*>(msg_payload), &completion_code,
            (bitfield8_t*)types);
        if (decode_return)
        {
            lprintf(LOG_ERR, " PLDM completion code - [0x%x]", decode_return);
            lprintf(LOG_ERR,
                    " Failed to decode message in GetPLDMTypes_rx_Callback");
            break;
        }
        if (completion_code)
        {
            lprintf(LOG_ERR, " Completion code - [0x%x]", completion_code);
            lprintf(LOG_ERR,
                    " Failed response message in GetPLDMTypes_rx_Callback");
            break;
        }
        loop_number = 0;
        for (int value : types)
        {
            lprintf(LOG_NOTICE, " Response Byte %d [%s]", loop_number++,
                    ((std::bitset<8>(value)).to_string()).c_str());
        }
    } while (false);
    if (msg_payload)
    {
        free(msg_payload);
    }
}

/*
 * Handles the GetPLDMCommands response from MTCP via SERIAL
 */
static void GetPLDMCommands_rx_Callback(uint8_t eid, void* data, void* msg,
                                        size_t len)
{
    struct pldm_msg_payload* msg_payload = NULL;
    lprintf(LOG_DEBUG,
            "Input Parameters eid [%d] data [0x%x] msg [0x%x] len [%d]", eid,
            data, msg, len);
    do
    {
        if (eid)
        {
            lprintf(LOG_ERR, " Error Received in GetPLDMCommands_rx_Callback");
            break;
        }
        if (!msg)
        {
            lprintf(LOG_ERR,
                    " Null message Received in GetPLDMCommands_rx_Callback");
            break;
        }
        if (PLDM_GET_COMMANDS_RESP_BYTES > len)
        {
            lprintf(LOG_ERR, " Invalid message size Received in "
                             "GetPLDMCommands_rx_Callback");
            break;
        }
        lprintf(LOG_INFO, " Successfully recieved message response ");
        // Create the response message
        int decode_return = 0;
        uint8_t completion_code = -1;
        msg_payload =
            (struct pldm_msg_payload*)malloc(sizeof(struct pldm_msg_payload));
        msg_payload->payload =
            static_cast<uint8_t*>(msg) + sizeof(pldm_msg_hdr);
        ;
        msg_payload->payload_length = PLDM_GET_COMMANDS_RESP_BYTES;
        uint8_t commands[PLDM_GET_COMMANDS_RESP_BYTES -
                         PLDM_COMPLETION_CODE_BYTES] = {0};
        int loop_number = 0;
        for (int value : commands)
        {
            lprintf(LOG_DEBUG, " Response Byte %d [%s]", loop_number++,
                    ((std::bitset<8>(value)).to_string()).c_str());
        }
        decode_return = decode_get_commands_resp(
            const_cast<struct pldm_msg_payload*>(msg_payload), &completion_code,
            (bitfield8_t*)commands);
        if (decode_return)
        {
            lprintf(LOG_ERR, " Decode return code - [0x%x]", decode_return);
            lprintf(LOG_ERR,
                    " Failed to decode message in GetPLDMTypes_rx_Callback");
            break;
        }
        if (completion_code)
        {
            lprintf(LOG_ERR, " Completion code - [0x%x]", completion_code);
            lprintf(LOG_ERR,
                    " Failed response message in GetPLDMTypes_rx_Callback");
            break;
        }
        loop_number = 0;
        for (int value : commands)
        {
            lprintf(LOG_NOTICE, " Response Byte %d [%s]", loop_number++,
                    ((std::bitset<8>(value)).to_string()).c_str());
        }
    } while (false);
    if (msg_payload)
    {
        free(msg_payload);
    }
}

/*
 * Handles the GetPLDMVersion response from MTCP via SERIAL
 */
static void GetPLDMVersion_rx_Callback(uint8_t eid, void* data, void* msg,
                                       size_t len)
{
    struct pldm_msg_payload* msg_payload = NULL;
    lprintf(LOG_DEBUG,
            "Input Parameters eid [%d] data [0x%x] msg [0x%x] len [%d]", eid,
            data, msg, len);
    do
    {
        if (eid)
        {
            lprintf(LOG_ERR, " Error Received in GetPLDMVersion_rx_Callback");
            break;
        }
        if (!msg)
        {
            lprintf(LOG_ERR,
                    " Null message Received in GetPLDMVersion_rx_Callback");
            break;
        }
        if (PLDM_GET_TYPES_RESP_BYTES > len)
        {
            lprintf(
                LOG_ERR,
                " Invalid message size Received in GetPLDMVersion_rx_Callback");
            break;
        }
        lprintf(LOG_INFO, " Successfully recieved message response ");
        // Create the response message
        int decode_return = 0;
        uint8_t completion_code = -1;
        uint32_t next_transfer_handle = 0;
        uint8_t transfer_flag = 0;
        ver32_t version = {0x00, 0x00, 0x00, 0x00};
        msg_payload =
            (struct pldm_msg_payload*)malloc(sizeof(struct pldm_msg_payload));
        msg_payload->payload =
            static_cast<uint8_t*>(msg) + sizeof(pldm_msg_hdr);
        ;
        msg_payload->payload_length = len - sizeof(pldm_msg_hdr);
        decode_return = decode_get_version_resp(
            const_cast<struct pldm_msg_payload*>(msg_payload), &completion_code,
            &next_transfer_handle, &transfer_flag, &version);
        if (decode_return)
        {
            lprintf(LOG_ERR, " PLDM completion code - [0x%x]", decode_return);
            lprintf(LOG_ERR,
                    " Failed to decode message in GetPLDMTypes_rx_Callback");
            break;
        }
        if (completion_code)
        {
            lprintf(LOG_ERR, " Completion code - [0x%x]", completion_code);
            lprintf(LOG_ERR,
                    " Failed response message in GetPLDMTypes_rx_Callback");
            break;
        }
        lprintf(LOG_DEBUG,
                "GetPLDMCommands Request message next_transfer_handle [0x%X]",
                next_transfer_handle);
        lprintf(LOG_DEBUG,
                "GetPLDMCommands Request message transfer_flag [0x%X]",
                transfer_flag);
        if (PLDM_START_AND_END != transfer_flag)
        {
            lprintf(LOG_ERR, " Part message received in "
                             "GetPLDMTypes_rx_Callback is NOT supported");
            break;
        }
        lprintf(LOG_NOTICE,
                "GetPLDMCommands Request message version major [%x]",
                version.major);
        lprintf(LOG_NOTICE,
                "GetPLDMCommands Request message version minor [%x]",
                version.minor);
        lprintf(LOG_NOTICE,
                "GetPLDMCommands Request message version update [%x]",
                version.update);
        lprintf(LOG_NOTICE,
                "GetPLDMCommands Request message version alpha [%x]",
                version.alpha);
    } while (false);
    if (msg_payload)
    {
        free(msg_payload);
    }
}
/*
 * Handles the GetPLDMTypes command
 */
int pldm_get_types_req(char* intfname, int argc, char** argv)
{
    int rc = -1;
    uint8_t instance_id = PLDM_LOCAL_INSTANCE_ID;
    struct pldm_msg* req_msg = NULL;

    lprintf(LOG_DEBUG, "Invoking GetPLDMTypes message via [%s]", intfname);
    do
    {
        // return if no interface mentioned
        if (NULL == intfname)
        {
            lprintf(LOG_ERR, " Failed to find an interface to use");
            rc = -1;
            break;
        }

        // Create the request message
        req_msg = (struct pldm_msg*)malloc(sizeof(struct pldm_msg));
        req_msg->body.payload = NULL;
        req_msg->body.payload_length = 0;
        rc = encode_get_types_req(instance_id, req_msg);
        if (rc)
        {
            lprintf(LOG_ERR, " PLDM completion code - [0x%x]", rc);
            lprintf(
                LOG_ERR,
                " Failed to encode the request message for GetPLDMCommands");
            rc = -1;
            break;
        }
        lprintf(LOG_DEBUG, "GetPLDMTypes: Ecoded request successfully");

        // Send the request message to receive the response
        if (strncmp("SELF", intfname, strlen(intfname)) == 0)
        {
            // Create the response message
            struct pldm_msg* resp_msg = NULL;
            resp_msg = (struct pldm_msg*)malloc(sizeof(struct pldm_msg));
            uint8_t resp_payload[PLDM_GET_TYPES_RESP_BYTES] = {0};
            resp_msg->body.payload = resp_payload;
            resp_msg->body.payload_length = sizeof(resp_payload);
            int loop_number = 0;
            for (int value : resp_payload)
            {
                lprintf(LOG_DEBUG, "GetPLDMTypes Response Byte %d [%d]",
                        loop_number++, value);
            }

            lprintf(LOG_DEBUG,
                    "GetPLDMTypes: created response buffer successfully");
            // using loopback for sending message and receiving response
            lprintf(LOG_DEBUG, "GetPLDMTypes sending request via loopback ");
            pldm::responder::getPLDMTypes(NULL, resp_msg);
            lprintf(LOG_DEBUG,
                    "GetPLDMTypes received libresponder response message");
            lprintf(LOG_DEBUG,
                    "GetPLDMTypes Printing libresponder response message");
            loop_number = 0;
            for (int value : resp_payload)
            {
                lprintf(LOG_NOTICE, "GetPLDMTypes Response Byte %d [%d]",
                        loop_number++, value);
            }
            lprintf(LOG_DEBUG, "GetPLDMTypes Response payload length [%x]",
                    resp_msg->body.payload_length);
            if (resp_msg)
            {
                free(resp_msg);
            }
        }
        else if (strncmp("SERIAL", intfname, strlen(intfname)) == 0)
        {
            // using MCTP for sending message and receiving response
            lprintf(LOG_DEBUG,
                    "GetPLDMTypes sending request to MCTP via serial ");
            // initialize and set up MCTP
            struct mctp* mctp[2];
            mctp[0] = mctp_init();
            if (!mctp[0])
            {
                lprintf(LOG_ERR, "GetPLDMTypes Failed to init mctp[0] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, "GetPLDMTypes intialized mctp[0] ");
            // assert(mctp[0]);
            mctp[1] = mctp_init();
            if (!mctp[1])
            {
                lprintf(LOG_ERR, "GetPLDMTypes Failed to init mctp[1] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, "GetPLDMTypes intialized mctp[1] ");

            struct mctp_binding_serial* serial[2];
            serial[0] = mctp_serial_init();
            if (!serial[0])
            {
                lprintf(LOG_ERR, "GetPLDMTypes Failed to init serial[0] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, "GetPLDMTypes intialized serial[0] ");
            serial[1] = mctp_serial_init();
            if (!serial[1])
            {
                lprintf(LOG_ERR, "GetPLDMTypes Failed to init serial[1] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, "GetPLDMTypes intialized serial[1] ");

            auto requests = "/dev/pldmin";
            auto responses = "/dev/pldmout";

            auto fdOut = open(requests, O_WRONLY);
            lprintf(LOG_DEBUG, "GetPLDMTypes opened fdOut ");
            mctp_serial_open_fd(serial[0], fdOut);
            lprintf(LOG_DEBUG, "GetPLDMTypes opened serial fdOut open ");
            mctp_serial_register_bus(serial[0], mctp[0], 0);
            lprintf(LOG_DEBUG, "GetPLDMTypes  regestered serial bus for fdOut");

            auto fdIn = open(responses, O_RDONLY);
            lprintf(LOG_DEBUG, "GetPLDMTypes opened fdIn ");
            mctp_serial_open_fd(serial[1], fdIn);
            lprintf(LOG_DEBUG, "GetPLDMTypes opened serial fdIn open  ");
            mctp_serial_register_bus(serial[1], mctp[1], 0);
            lprintf(LOG_DEBUG, "GetPLDMTypes  regestered serial bus for fdIn");

            mctp_set_rx_all(mctp[1], GetPLDMTypes_rx_Callback, NULL);
            lprintf(LOG_DEBUG, "GetPLDMTypes  setup callback successfull");

            // create the pldm request packet
            uint8_t pldm_pkt_length = sizeof(struct pldm_msg_hdr);
            uint8_t* pldm_pkt = (uint8_t*)malloc(pldm_pkt_length);
            memcpy(pldm_pkt, &req_msg->hdr, sizeof(struct pldm_msg_hdr));

            mctp_message_tx(mctp[0], 0, pldm_pkt, pldm_pkt_length);
            lprintf(LOG_INFO, "Successfully transmitted message");

            mctp_serial_read(serial[1]);
            lprintf(LOG_DEBUG, "GetPLDMTypes  serial read triggered");
            close(fdIn);
            close(fdOut);
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

    lprintf(LOG_DEBUG, "pldm_get_types_req exiting with rc [%d]", rc);
    return rc;
}

/*
 * Handles the GetPLDMCommands command
 */
int pldm_get_commands(char* intfname, int argc, char** argv)
{
    int rc = -1;
    uint8_t instance_id = PLDM_LOCAL_INSTANCE_ID;
    struct pldm_msg* req_msg = NULL;
    struct pldm_msg* resp_msg = NULL;
    int argflag = -1;
    uint32_t input_version = 0xFFFFFFFF;

    lprintf(LOG_DEBUG, " Entered with interface [%s]", intfname);

    while (argc > 0)
    {
        if (0 == strncmp(argv[argflag], "-t", 2))
        {
            argflag++;
            lprintf(LOG_DEBUG, " string [%s] ", argv[argflag]);
            input_version = strtoul(argv[argflag], NULL, 16);
            lprintf(LOG_DEBUG, " input_version [0x%X] ", input_version);
        }
        argc--;
        argflag++;
    }
    do
    {
        if (0xFFFFFFFF == input_version)
        {

            lprintf(LOG_ERR, " Missing mandatory -t option ");
            rc = -1;
            break;
        }
        // PLDM Type Code  000000b from DSP0245 for valid PLDMType values.
        uint8_t pldmtype = 0x0;
        ver32_t version = {0};
        version.major = (input_version & 0xFF000000) >> 24;
        version.minor = (input_version & 0x00FF0000) >> 16;
        version.update = (input_version & 0x0000FF00) >> 8;
        version.alpha = input_version & 0x000000FF;
        uint8_t* req_payload = (uint8_t*)malloc(PLDM_GET_COMMANDS_REQ_BYTES);
        req_msg = (struct pldm_msg*)malloc(sizeof(struct pldm_msg));
        req_msg->body.payload = req_payload;
        req_msg->body.payload_length = PLDM_GET_COMMANDS_REQ_BYTES;
        rc = encode_get_commands_req(instance_id, pldmtype, version, req_msg);
        if (rc > 0)
        {
            lprintf(
                LOG_ERR,
                " Failed to encode the request message for GetPLDMCommands");
            rc = -1;
            break;
        }
        lprintf(LOG_DEBUG, "GetPLDMCommands ecoded request successfully");
        lprintf(LOG_DEBUG, "GetPLDMCommands detailed request message asked");
        lprintf(LOG_DEBUG, "GetPLDMCommands Request message type [%d]",
                pldmtype);
        lprintf(LOG_DEBUG, "GetPLDMCommands Request message version major [%x]",
                version.major);
        lprintf(LOG_DEBUG, "GetPLDMCommands Request message version minor [%x]",
                version.minor);
        lprintf(LOG_DEBUG,
                "GetPLDMCommands Request message version update [%x]",
                version.update);
        lprintf(LOG_DEBUG, "GetPLDMCommands Request message version alpha [%x]",
                version.alpha);
        // Send the request
        if (strncmp("SELF", intfname, strlen(intfname)) == 0)
        {
            lprintf(LOG_DEBUG,
                    "GetPLDMCommands direct libresponder usage asked");
            resp_msg = (struct pldm_msg*)malloc(sizeof(struct pldm_msg));
            uint8_t resp_payload[PLDM_GET_COMMANDS_RESP_BYTES] = {0};
            resp_msg->body.payload = resp_payload;
            resp_msg->body.payload_length = PLDM_GET_COMMANDS_RESP_BYTES;
            lprintf(LOG_DEBUG,
                    "GetPLDMCommands response message before sending");
            int loop_number = 0;
            for (int value : resp_payload)
            {
                lprintf(LOG_DEBUG, "GetPLDMCommands Response Byte %d [%s]",
                        loop_number++,
                        ((std::bitset<8>(value)).to_string()).c_str());
            }
            lprintf(LOG_DEBUG, "GetPLDMCommands Response payload length [%d]",
                    resp_msg->body.payload_length);
            lprintf(LOG_DEBUG,
                    "GetPLDMCommands sending request message to libresponder ");
            pldm::responder::getPLDMCommands((&req_msg->body), resp_msg);
            lprintf(LOG_DEBUG,
                    "GetPLDMCommands received libresponder response message");
            lprintf(LOG_DEBUG,
                    "GetPLDMCommands Printing libresponder response message");
            loop_number = 0;
            for (int value : resp_payload)
            {
                lprintf(LOG_NOTICE, "GetPLDMCommands Response Byte %d [%s]",
                        loop_number++,
                        ((std::bitset<8>(value)).to_string()).c_str());
            }
            lprintf(LOG_DEBUG, "GetPLDMCommands Response payload length [%d]",
                    resp_msg->body.payload_length);
        }
        else if (strncmp("SERIAL", intfname, strlen(intfname)) == 0)
        {
            // using MCTP for sending message and receiving response
            lprintf(LOG_DEBUG,
                    "GetPLDMCommands sending request to MCTP via serial ");
            // initialize mctp
            struct mctp* mctp[2];
            mctp[0] = mctp_init();
            if (!mctp[0])
            {
                lprintf(LOG_ERR, "GetPLDMCommands Failed to init mctp[0] ");
                rc = -1;
                break;
            }
            lprintf(LOG_ERR, "GetPLDMCommands intialized mctp[0] ");
            // assert(mctp[0]);
            mctp[1] = mctp_init();
            if (!mctp[1])
            {
                lprintf(LOG_ERR, "GetPLDMCommands Failed to init mctp[1] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, "GetPLDMCommands intialized mctp[1] ");

            struct mctp_binding_serial* serial[2];
            serial[0] = mctp_serial_init();
            if (!serial[0])
            {
                lprintf(LOG_ERR, "GetPLDMCommands Failed to init serial[0] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, "GetPLDMCommands intialized serial[0] ");
            serial[1] = mctp_serial_init();
            if (!serial[1])
            {
                lprintf(LOG_ERR, "GetPLDMCommands Failed to init serial[1] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, "GetPLDMCommands intialized serial[1] ");

            auto requests = "/dev/pldmin";
            auto responses = "/dev/pldmout";

            auto fdOut = open(requests, O_WRONLY);
            lprintf(LOG_DEBUG, "GetPLDMCommands opened fdOut ");
            mctp_serial_open_fd(serial[0], fdOut);
            lprintf(LOG_DEBUG, "GetPLDMCommands opened serial fdOut open ");
            mctp_serial_register_bus(serial[0], mctp[0], 0);
            lprintf(LOG_DEBUG,
                    "GetPLDMCommands  regestered serial bus for fdOut");

            auto fdIn = open(responses, O_RDONLY);
            lprintf(LOG_DEBUG, "GetPLDMCommands opened fdIn ");
            mctp_serial_open_fd(serial[1], fdIn);
            lprintf(LOG_DEBUG, "GetPLDMCommands opened serial fdIn open  ");
            mctp_serial_register_bus(serial[1], mctp[1], 0);
            lprintf(LOG_DEBUG,
                    "GetPLDMCommands  regestered serial bus for fdIn");
            mctp_set_rx_all(mctp[1], GetPLDMCommands_rx_Callback, NULL);

            // create the pldm request packet
            uint8_t pldm_pkt_length =
                sizeof(struct pldm_msg_hdr) + PLDM_GET_COMMANDS_REQ_BYTES;
            uint8_t* pldm_pkt = (uint8_t*)malloc(pldm_pkt_length);
            memcpy(pldm_pkt, &req_msg->hdr, sizeof(struct pldm_msg_hdr));
            memcpy(pldm_pkt + sizeof(struct pldm_msg_hdr), req_payload,
                   PLDM_GET_COMMANDS_REQ_BYTES);

            lprintf(LOG_DEBUG, "GetPLDMCommands  setup callback successfull");
            mctp_message_tx(mctp[0], 0, pldm_pkt, pldm_pkt_length);
            lprintf(LOG_INFO, "Successfully transmitted message");
            mctp_serial_read(serial[1]);
            lprintf(LOG_DEBUG, "GetPLDMCommands  serial read triggered");
            close(fdIn);
            close(fdOut);
        }
        else if (strncmp("DBUS", intfname, strlen(intfname)) == 0)
        {
            lprintf(LOG_DEBUG, "GetPLDMCommands DBUS support to be added");
            lprintf(LOG_DEBUG, "GetPLDMCommands DBUS currently unsupported");
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

    lprintf(LOG_DEBUG, "pldm_get_commands entered with rc [%d]", rc);
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
    uint8_t instance_id = PLDM_LOCAL_INSTANCE_ID;
    struct pldm_msg* req_msg = NULL;
    struct pldm_msg* resp_msg = NULL;
    uint32_t transfer_handle = 0x0;
    transfer_op_flag transfer_operation_flag = PLDM_GET_FIRSTPART;

    lprintf(LOG_DEBUG, " Entered with interface [%s]", intfname);
    do
    {
        // PLDM Type Code  000000b from DSP0245 for valid PLDMType values.
        uint8_t pldmtype = 0x0;
        uint8_t* req_payload = (uint8_t*)malloc(PLDM_GET_VERSION_REQ_BYTES);
        req_msg = (struct pldm_msg*)malloc(sizeof(struct pldm_msg));
        req_msg->body.payload = req_payload;
        req_msg->body.payload_length = PLDM_GET_VERSION_REQ_BYTES;
        rc = encode_get_version_req(instance_id, transfer_handle,
                                    transfer_operation_flag, pldmtype, req_msg);
        if (rc > 0)
        {
            lprintf(LOG_ERR,
                    " Failed to encode the request message for GetPLDMVersion");
            rc = -1;
            break;
        }
        lprintf(LOG_DEBUG, " Ecoded request successfully");
        lprintf(LOG_DEBUG, " Request message type [%d]", pldmtype);
        lprintf(LOG_DEBUG, " Request message transfer operation flag [%d]",
                transfer_operation_flag);
        lprintf(LOG_DEBUG, " Request message transfer handle [%d]",
                transfer_handle);
        // Send the request
        if (strncmp("SELF", intfname, strlen(intfname)) == 0)
        {
            lprintf(LOG_DEBUG,
                    "GetPLDMVersion direct libresponder usage asked");
            resp_msg = (struct pldm_msg*)malloc(sizeof(struct pldm_msg));
            uint8_t resp_payload[PLDM_GET_VERSION_RESP_BYTES] = {0};
            resp_msg->body.payload = resp_payload;
            resp_msg->body.payload_length = PLDM_GET_VERSION_RESP_BYTES;
            lprintf(LOG_DEBUG,
                    "GetPLDMVersion response message before sending");
            int loop_number = 0;
            for (int value : resp_payload)
            {
                lprintf(LOG_DEBUG, "GetPLDMVersion Response Byte %d [%s]",
                        loop_number++,
                        ((std::bitset<8>(value)).to_string()).c_str());
            }
            lprintf(LOG_DEBUG, "GetPLDMVersion Response payload length [%d]",
                    resp_msg->body.payload_length);
            lprintf(LOG_DEBUG,
                    "GetPLDMVersion sending request message to libresponder ");
            pldm::responder::getPLDMVersion((&req_msg->body), resp_msg);
            lprintf(LOG_DEBUG,
                    "GetPLDMVersion received libresponder response message");
            lprintf(LOG_DEBUG,
                    "GetPLDMVersion Printing libresponder response message");
            loop_number = 0;
            for (int value : resp_payload)
            {
                lprintf(LOG_DEBUG, "GetPLDMVersion Response Byte %d [%s]",
                        loop_number++,
                        ((std::bitset<8>(value)).to_string()).c_str());
            }
            lprintf(LOG_NOTICE,
                    "GetPLDMVersion Request message version major [%x]",
                    resp_payload[6]);
            lprintf(LOG_NOTICE,
                    "GetPLDMVersion Request message version minor [%x]",
                    resp_payload[7]);
            lprintf(LOG_NOTICE,
                    "GetPLDMVersion Request message version update [%x]",
                    resp_payload[8]);
            lprintf(LOG_NOTICE,
                    "GetPLDMVersion Request message version alpha [%x]",
                    resp_payload[9]);
            lprintf(LOG_DEBUG, "GetPLDMVersion Response payload length [%d]",
                    resp_msg->body.payload_length);
        }
        else if (strncmp("SERIAL", intfname, strlen(intfname)) == 0)
        {
            // using MCTP for sending message and receiving response
            lprintf(LOG_DEBUG, " Sending request to MCTP via serial ");
            // initialize mctp
            struct mctp* mctp[2];
            mctp[0] = mctp_init();
            if (!mctp[0])
            {
                lprintf(LOG_ERR, " Failed to init mctp[0] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, " Intialized mctp[0] ");
            // assert(mctp[0]);
            mctp[1] = mctp_init();
            if (!mctp[1])
            {
                lprintf(LOG_ERR, " Failed to init mctp[1] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, " intialized mctp[1] ");

            struct mctp_binding_serial* serial[2];
            serial[0] = mctp_serial_init();
            if (!serial[0])
            {
                lprintf(LOG_ERR, " Failed to init serial[0] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, " intialized serial[0] ");
            serial[1] = mctp_serial_init();
            if (!serial[1])
            {
                lprintf(LOG_ERR, " Failed to init serial[1] ");
                rc = -1;
                break;
            }
            lprintf(LOG_DEBUG, " intialized serial[1] ");

            auto requests = "/dev/pldmin";
            auto responses = "/dev/pldmout";

            auto fdOut = open(requests, O_WRONLY);
            lprintf(LOG_DEBUG, " opened fdOut ");
            mctp_serial_open_fd(serial[0], fdOut);
            lprintf(LOG_DEBUG, " opened serial fdOut open ");
            mctp_serial_register_bus(serial[0], mctp[0], 0);
            lprintf(LOG_DEBUG, "  regestered serial bus for fdOut");

            auto fdIn = open(responses, O_RDONLY);
            lprintf(LOG_DEBUG, " opened fdIn ");
            mctp_serial_open_fd(serial[1], fdIn);
            lprintf(LOG_DEBUG, " opened serial fdIn open  ");
            mctp_serial_register_bus(serial[1], mctp[1], 0);
            lprintf(LOG_DEBUG, "  regestered serial bus for fdIn");
            mctp_set_rx_all(mctp[1], GetPLDMVersion_rx_Callback, NULL);
            lprintf(LOG_DEBUG, "  setup callback successfull");

            // create the pldm request packet
            uint8_t pldm_pkt_length =
                sizeof(struct pldm_msg_hdr) + PLDM_GET_VERSION_REQ_BYTES;
            uint8_t* pldm_pkt = (uint8_t*)malloc(pldm_pkt_length);
            memcpy(pldm_pkt, &req_msg->hdr, sizeof(struct pldm_msg_hdr));
            memcpy(pldm_pkt + sizeof(struct pldm_msg_hdr), req_payload,
                   PLDM_GET_VERSION_REQ_BYTES);

            mctp_message_tx(mctp[0], 0, pldm_pkt, pldm_pkt_length);
            lprintf(LOG_INFO, "Successfully transmitted message");
            mctp_serial_read(serial[1]);
            lprintf(LOG_DEBUG, "GetPLDMVersion  serial read triggered");
            close(fdIn);
            close(fdOut);
        }
        else if (strncmp("DBUS", intfname, strlen(intfname)) == 0)
        {
            lprintf(LOG_DEBUG, "GetPLDMVersion DBUS support to be added");
            lprintf(LOG_DEBUG, "GetPLDMVersion DBUS currently unsupported");
        }
        else
        {
            lprintf(LOG_ERR, "Unknown Interface");
            rc = -1;
            break;
        }
    } while (false);
    return rc;
}
