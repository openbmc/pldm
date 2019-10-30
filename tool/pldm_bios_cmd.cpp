#include "pldm_bios_cmd.hpp"

#include "pldm_cmd_helper.hpp"

#include <string>

constexpr uint8_t PLDM_ENTITY_ID = 8;
constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;
constexpr uint8_t PLDM_LOCAL_INSTANCE_ID = 0;

using namespace std;

void getBIOSTable(vector<std::string>&& args)

{
    // Create and encode the request message
    vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                               PLDM_GET_BIOS_TABLE_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    uint8_t instanceId = 0;
    uint32_t nextTransferHandle = 32;
    uint8_t transferOpFlag = PLDM_GET_FIRSTPART;
    uint8_t tableType = 0;

    if (!args[1].c_str())
    {
        cout << "Mandatory argument PLDM Command Type not provided!" << endl;
        cout << "Run pldmtool --help for more information" << endl;
        return;
    }

    if (!strcasecmp(args[1].c_str(), "attributeTable"))
    {
        cout << "PLDM Type requested : " << args[1] << endl;
        tableType = PLDM_BIOS_ATTR_TABLE;
    }
    else if (!strcasecmp(args[1].c_str(), "stringTable"))
    {
        cout << "PLDM Type requested : " << args[1] << endl;
        tableType = PLDM_BIOS_STRING_TABLE;
    }
    else
    {
        cerr << "Unsupported pldm command type OR not supported yet : "
             << args[1] << endl;
        return;
    }

    // Encode get_bios_table_req types
    auto returnCode = encode_get_bios_table_req(
        instanceId, nextTransferHandle, transferOpFlag, tableType, request);
    if (returnCode)
    {
        cerr << "Failed to encode request msg for GetBIOSTable : RC = "
             << returnCode << endl;
        return;
    }
    cout << "Encoded request succesfully : RC = " << returnCode << endl;

    cout << "Before doing insert : " << endl;
    printBuffer(requestMsg);

    // Insert the PLDM message type and EID at the begining of the request msg.
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), PLDM_ENTITY_ID);

    cout << "Request Message:" << endl;
    printBuffer(requestMsg);

    // Create the response message
    vector<uint8_t> responseMsg;

    /* Compares the response with request packet on first socket recv() call.
       If above condition is qualified then, reads the actual response from
       the socket to output buffer responseMsg.*/
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

void getDateTime(vector<std::string>&&)

{
    // Create a request packet
    vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                               PLDM_GET_BIOS_TABLE_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    uint8_t instanceId = 0;

    // encode the get_version request
    auto returnCode = encode_get_date_time_req(instanceId, request);

    if (returnCode)
    {
        cerr << "Failed to encode request msg for GetBIOSTable : RC = "
             << returnCode << endl;
        return;
    }
    cout << "Encoded request succesfully : RC = " << returnCode << endl;

    // Insert the PLDM message type and EID at the begining of the request msg.
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), PLDM_ENTITY_ID);

    cout << "Request Message:" << endl;

    // Create the response message
    vector<uint8_t> responseMsg;

    /* Compares the response with request packet on first socket recv() call.
       If above condition is qualified then, reads the actual response from
       the socket to output buffer responseMsg.*/
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
