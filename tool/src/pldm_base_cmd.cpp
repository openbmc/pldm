#include "pldm_base_cmd.hpp"

#include "pldm_cmd_helper.hpp"

constexpr uint8_t PLDM_ENTITY_ID = 8;
constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;

/*
 * Main function that handles the GetPLDMTypes response callback via mctp
 *
 */
void GetPLDMTypes()
{
    int socketFd = -1;
    int result = -1;
    int returnCode = 0;

    // Initialize the mctp device
    socketFd = mctp_mux_init();
    if (socketFd <= 0)
    {
        Logger::Error(" MCTP initialization failed : RC = [%d]", socketFd);
        return;
    }
    Logger::Debug("MCTP initialization Success : RC = [%d]", socketFd);

    // Create and encode the request message
    uint8_t instance_id = PLDM_LOCAL_INSTANCE_ID;
    std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                    PLDM_GET_COMMANDS_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    result = encode_get_types_req(instance_id, request);
    if (result)
    {
        Logger::Error("Failed to encode the req message for GetPLDMType."
                      "result = [%d]",
                      result);
        close(socketFd);
        return;
    }
    Logger::Debug("Encoded request succesfully : RC = [%d]", result);

    // Insert the PLDM message type and EID at the begining of the request msg.
    uint8_t eid = PLDM_ENTITY_ID;
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), eid);

    Logger::Debug("\nGetPLDMType Request Data: ");
    for (size_t i = 0; i < requestMsg.size(); i++)
    {
        Logger::Info("%02x ", *((uint8_t*)requestMsg.data() + i));
    }

    result = send(socketFd, requestMsg.data(),
                  sizeof(pldm_msg_hdr) + PLDM_GET_COMMANDS_REQ_BYTES, 0);
    if (-1 == result)
    {
        returnCode = -errno;
        Logger::Error("Write to socket failure : RC = [%d]", returnCode);
        close(socketFd);
        return;
    }
    Logger::Debug("Write to socket successful : RC = [%d]", result);

    // Read the response from socket
    Logger::Debug("Waiting for the data from socket :");
    ssize_t peekedLength = recv(socketFd, nullptr, 0, MSG_TRUNC | MSG_PEEK);
    if (0 == peekedLength)
    {
        Logger::Error("Socket has been closed : peekedLength = [%d]",
                      peekedLength);
        close(socketFd);
        return;
    }
    else if (peekedLength <= -1)
    {
        returnCode = -errno;
        Logger::Error("recv() system call failed, RC = [%d]", returnCode);
        close(socketFd);
        return;
    }
    else
    {
        // loopback response message
        std::vector<uint8_t> lbrespMsg(peekedLength);
        auto recvDataLength =
            recv(socketFd, reinterpret_cast<void*>(lbrespMsg.data()),
                 peekedLength, 0);
        if (recvDataLength == peekedLength)
        {
            // print the recieve buffer
            Logger::Info("Total length [%zu]", recvDataLength);
            Logger::Info("Recieved Data:");
            for (int i = 0; i < recvDataLength; i++)
            {
                Logger::Info("%02x ", *((uint8_t*)lbrespMsg.data() + i));
            }
        }
        else
        {
            Logger::Error("Failure to read peeked length packet");
            close(socketFd);
            return;
        }

        // Comparing if the data recieved on first attempt Vs. request sent
        auto result =
            std::memcmp(requestMsg.data(), lbrespMsg.data(), requestMsg.size());

        // If response = request in first recv() attempt then, read again
        // to get actual command response
        auto respEid = *((uint8_t*)lbrespMsg.data() + 0);
        auto respType = *((uint8_t*)lbrespMsg.data() + 1);
        Logger::Info("Entity ID = %d", respEid);
        Logger::Info("PLDM Type = %d", respType);
        Logger::Info("memcmp result = %d", result);
        if ((!result) &&
            (respEid == PLDM_ENTITY_ID && respType == MCTP_MSG_TYPE_PLDM))
        {

            Logger::Debug("First response message is equal to request message");
            ssize_t peekedLength =
                recv(socketFd, nullptr, 0, MSG_PEEK | MSG_TRUNC);
            std::vector<uint8_t> responseMsg(peekedLength);
            recvDataLength =
                recv(socketFd, reinterpret_cast<void*>(responseMsg.data()),
                     peekedLength, 0);
            if (recvDataLength == peekedLength)
            {
                // print the recieve buffer
                Logger::Info("Total length [%zu]", recvDataLength);
                Logger::Info("Actual Recieved Data:");
                for (int i = 0; i < recvDataLength; i++)
                {
                    Logger::Info("%02x ", *((uint8_t*)responseMsg.data() + i));
                }
            }
            else
            {
                Logger::Error("Failure to read response length packet");
                close(socketFd);
                return;
            }
        }
        else
        {
            Logger::Error("requestMsg != responseMsg");
            close(socketFd);
            return;
        }
    }

    returnCode = shutdown(socketFd, SHUT_RDWR);
    if (-1 == returnCode)
    {
        returnCode = -errno;
        Logger::Error("Failed to shutdown the socket, RC=%d", returnCode);
        close(socketFd);
        return;
    }
}

REGISTER_CALLBACK("GetPLDMTypes", GetPLDMTypes);
