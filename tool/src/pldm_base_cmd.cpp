#include "pldm_base_cmd.hpp"

#include "pldm_cmd_helper.hpp"

#include <string>

constexpr uint8_t PLDM_ENTITY_ID = 8;
constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;
constexpr uint8_t PLDM_LOCAL_INSTANCE_ID = 0;

using namespace std;

/*
 * Main function that handles the GetPLDMTypes response callback via mctp
 *
 */
void getPLDMTypes(vector<std::string>&& args)
{
    // Create and encode the request message
    vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                               sizeof(MCTP_MSG_TYPE_PLDM) +
                               sizeof(PLDM_ENTITY_ID));
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // Encode the get_types request message
    uint8_t instanceId = PLDM_LOCAL_INSTANCE_ID;
    auto returnCode = encode_get_types_req(instanceId, request);
    if (returnCode)
    {
        cerr << "Failed to encode request msg for GetPLDMType : RC = "
             << returnCode << endl;
        return;
    }
    cout << "Encoded request succesfully : RC = " << returnCode << endl;

    // Insert the PLDM message type and EID at the begining of the request msg.
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), PLDM_ENTITY_ID);

    cout << "Request Message:" << endl;
    printBuffer(requestMsg);

    // Create the response message
    vector<uint8_t> responseMsg;

    // Compares the response with request packet on first socket recv() call.
    // If above condition is qualified then, reads the actual response from
    // the socket to output buffer responseMsg.
    returnCode = mctpSockSendRecv(requestMsg, responseMsg);
    if (!returnCode)
    {
        cout << "Socket recv() successful : RC = " << returnCode << endl;
        cout << "Response Message : " << endl;
        printBuffer(responseMsg);
    }
    else
    {
        cerr << "Failed to recieve from socket : RC = " << returnCode << endl;
        return;
    }
}

/*
 * Main function that handles the GetPLDMVersion response callback via mctp
 *
 */
void getPLDMVersion(vector<std::string>&& args)
{
    // Create a request packet
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_GET_VERSION_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    uint8_t pldmType = 0x0;

    if (!args[1].c_str())
    {
        cout << "Mandatory argument PLDM Command Type not provided!" << endl;
        cout << "Run pldmtool --help for more information" << endl;
        return;
    }

    if (!strcasecmp(args[1].c_str(), "base"))
    {
        cout << "PLDM Type requested : " << args[1] << endl;
        pldmType = PLDM_BASE;
    }
    else
    {
        cerr << "Unsupported pldm command type OR not supported yet : "
             << args[1] << endl;
        return;
    }

    uint8_t instanceId = PLDM_LOCAL_INSTANCE_ID;
    uint32_t transferHandle = 0x0;
    transfer_op_flag opFlag = PLDM_GET_FIRSTPART;

    // encode the get_version request
    auto returnCode = encode_get_version_req(instanceId, transferHandle, opFlag,
                                             pldmType, request);
    if (returnCode)
    {
        cerr << "Failed to encode request msg for GetPLDMVersion. RC = "
             << returnCode << endl;
        return;
    }
    cout << "Encoded request succesfully : RC = " << returnCode << endl;

    // Insert the PLDM message type and EID at the begining of the request msg.
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), PLDM_ENTITY_ID);

    cout << "Request Message:" << endl;
    printBuffer(requestMsg);

    // Create the response message
    vector<uint8_t> responseMsg;

    // Compares the response with request packet on first socket recv() call.
    // If above condition is qualified then, reads the actual response from
    // the socket to output buffer responseMsg.
    returnCode = mctpSockSendRecv(requestMsg, responseMsg);
    if (!returnCode)
    {
        cout << "Socket recv() successful : RC = " << returnCode << endl;
        cout << "Response Message:" << endl;
        printBuffer(responseMsg);
    }
    else
    {
        cerr << "Failed to recieve from socket : RC = " << returnCode << endl;
        return;
    }
}
