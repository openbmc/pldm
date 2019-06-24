#include "inc/pldm_base_cmd.hpp"

#include "inc/pldm_cmd_helper.hpp"

constexpr uint8_t PLDM_ENTITY_ID = 8;
constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;

/*
 * Main function that handles the GetPLDMTypes response callback via mctp
 *
 */
void GetPLDMTypes()
{
    int returnCode = 0;

    // Initialize the mctp device
    int socketFd = mctp_sock_init();
    if (socketFd <= 0)
    {
        LOG_E(" MCTP initialization failed : RC = [%d]", socketFd);
        return;
    }
    LOG_D("MCTP initialization Success : RC = [%d]", socketFd);

    // Create and encode the request message
    uint8_t instance_id = PLDM_LOCAL_INSTANCE_ID;
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    sizeof(MCTP_MSG_TYPE_PLDM) +
                                    sizeof(PLDM_ENTITY_ID));
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // Encode the get_types request message
    returnCode = encode_get_types_req(instance_id, request);
    if (returnCode)
    {
        LOG_E("Failed to encode the req message for GetPLDMType."
              "RC = [%d]",
              returnCode);
        close(socketFd);
        return;
    }
    LOG_D("Encoded request succesfully : RC = [%d]", returnCode);

    // Insert the PLDM message type and EID at the begining of the request msg.
    uint8_t eid = PLDM_ENTITY_ID;
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), eid);

    Logger::Info("Request Message:");
    printBuffer(requestMsg);

    int result = -1;
    result = send(socketFd, requestMsg.data(), requestMsg.size(), 0);
    if (-1 == result)
    {
        returnCode = -errno;
        LOG_E("Write to socket failure : RC = [%d]", returnCode);
        close(socketFd);
        return;
    }
    LOG_D("Write to socket successful : RC = [%d]", result);

    // Create the response message
    std::vector<uint8_t> responseMsg;

    // Compares the response with request packet on first socket recv() call.
    // If above condition is qualified then, reads the actual response from
    // the socket to output buffer responseMsg.
    returnCode = mctp_sock_recv(socketFd, requestMsg, responseMsg);
    if (!returnCode)
    {
        LOG_I("Socket recv() successful.\n"
              "Response Message:",
              returnCode);
        printBuffer(responseMsg);
    }
    else
    {
        LOG_E("Failed to recieve data from socket, RC=%d", returnCode);
        close(socketFd);
        return;
    }

    returnCode = shutdown(socketFd, SHUT_RDWR);
    if (-1 == returnCode)
    {
        returnCode = -errno;
        LOG_E("Failed to shutdown the socket, RC=%d", returnCode);
        close(socketFd);
        return;
    }
    LOG_D("Shutdown Socket successful, RC=%d", returnCode);
}

REGISTER_CALLBACK("GetPLDMTypes", GetPLDMTypes);

/*
 * Main function that handles the GetPLDMVersion response callback via mctp
 *
 */
void GetPLDMVersion()
{
    int returnCode = 0;

    // Initialize the mctp device
    int socketFd = mctp_sock_init();
    if (socketFd <= 0)
    {
        LOG_E(" MCTP initialization failed : RC = [%d]", socketFd);
        return;
    }
    LOG_D("MCTP initialization Success : RC = [%d]", socketFd);

    // Create the request message
    uint8_t instance_id = PLDM_LOCAL_INSTANCE_ID;
    uint32_t transferHandle = 0x0;
    transfer_op_flag opFlag = PLDM_GET_FIRSTPART;
    uint8_t pldmType = 0x0;

    // Create a request packet
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_GET_VERSION_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    // encode the get_version request
    returnCode = encode_get_version_req(instance_id, transferHandle, opFlag,
                                        pldmType, request);
    if (returnCode)
    {
        LOG_E("Failed to encode the req message for GetPLDMType."
              "RC = [%d]",
              returnCode);
        close(socketFd);
        return;
    }
    LOG_D("Encoded request succesfully : RC = [%d]", returnCode);

    // Insert the PLDM message type and EID at the begining of the request msg.
    uint8_t eid = PLDM_ENTITY_ID;
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), eid);

    Logger::Info("Request Message:");
    printBuffer(requestMsg);

    int result = -1;
    result = send(socketFd, requestMsg.data(), requestMsg.size(), 0);
    if (-1 == result)
    {
        returnCode = -errno;
        LOG_E("Write to socket failure : RC = [%d]", returnCode);
        close(socketFd);
        return;
    }
    LOG_D("Write to socket successful : RC = [%d]", result);

    // Create the response message
    std::vector<uint8_t> responseMsg;

    // Compares the response with request packet on first socket recv() call.
    // If above condition is qualified then, reads the actual response from
    // the socket to output buffer responseMsg.
    returnCode = mctp_sock_recv(socketFd, requestMsg, responseMsg);
    if (!returnCode)
    {
        LOG_I("Socket recv() successful.\n"
              "Response Message:",
              returnCode);
        printBuffer(responseMsg);
    }
    else
    {
        LOG_E("Failed to recieve data from socket, RC=%d", returnCode);
        close(socketFd);
        return;
    }

    returnCode = shutdown(socketFd, SHUT_RDWR);
    if (-1 == returnCode)
    {
        returnCode = -errno;
        LOG_E("Failed to shutdown the socket, RC=%d", returnCode);
        close(socketFd);
        return;
    }
    LOG_D("Shutdown Socket successful, RC=%d", returnCode);
}

REGISTER_CALLBACK("GetPLDMVersion", GetPLDMVersion);
