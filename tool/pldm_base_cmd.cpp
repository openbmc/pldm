#include "pldm_base_cmd.hpp"

#include "pldm_cmd_helper.hpp"

#include <map>
#include <string>

constexpr uint8_t PLDM_ENTITY_ID = 8;
constexpr uint8_t MCTP_MSG_TYPE_PLDM = 1;
constexpr uint8_t PLDM_LOCAL_INSTANCE_ID = 0;

class GetPLDMTypes : public CommandInterface
{
  public:
    ~GetPLDMTypes() = default;
    GetPLDMTypes(const GetPLDMTypes&) = default;
    GetPLDMTypes(GetPLDMTypes&&) = default;
    GetPLDMTypes& operator=(const GetPLDMTypes&) = default;
    GetPLDMTypes& operator=(GetPLDMTypes&&) = default;
    GetPLDMTypes() : CommandInterface("GetPLDMTypes")
    {
    }

    std::pair<int, std::vector<uint8_t>>
        createRequestMsg(std::vector<std::string>& args) override
    {
        std::ignore = args;
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        auto rc = encode_get_types_req(PLDM_LOCAL_INSTANCE_ID, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc;
        std::vector<bitfield8_t> types(8);
        auto rc = decode_get_types_resp(responsePtr, payloadLength, &cc,
                                        types.data());
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cout << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << cc << std::endl;
            return;
        }

        printPldmTypes(types);
    }

  private:
    void printPldmTypes(std::vector<bitfield8_t>& types)
    {
        const std::map<int, const char*> pldmTypeMap = {{0, "base"},
                                                        {2, "platform"},
                                                        {3, "bios"},
                                                        {4, "fru"},
                                                        {0x3f, "oem"}};
        std::cout << "Supported types:";
        for (int i = 0; i < PLDM_MAX_TYPES; i++)
        {
            bitfield8_t b = types[i / 8];
            if (b.byte & (1 << i % 8))
            {
                std::cout << " " << i;
                auto it = pldmTypeMap.find(i);
                if (it != pldmTypeMap.end())
                {

                    std::cout << "(" << it->second << ")";
                }
            }
        }

        std::cout << std::endl;
    }
};

class RawOp : public CommandInterface
{
  public:
    ~RawOp() = default;
    RawOp(const RawOp&) = default;
    RawOp(RawOp&&) = default;
    RawOp& operator=(const RawOp&) = default;
    RawOp& operator=(RawOp&&) = default;
    RawOp() : CommandInterface("RawOp")
    {
    }
    std::pair<int, std::vector<uint8_t>>
        createRequestMsg(std::vector<std::string>& args) override

    {
        if (args.size() < 3)
        {
            std::cerr << "Not enough arguments passed."
                         " Minimum, need to pass PLDM header raw data"
                      << std::endl;
            return {PLDM_ERROR_INVALID_DATA, {}};
        }
        std::vector<uint8_t> requestMsg;
        for (auto&& rawByte : args)
        {
            requestMsg.insert(requestMsg.end(), stoi(rawByte, nullptr, 16));
        }
        return {PLDM_SUCCESS, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        std::ignore = responsePtr;
        std::ignore = payloadLength;
    }
};

using namespace std;

/*
 * Main function that handles the GetPLDMTypes response callback via mctp
 *
 */
void getPLDMTypes(vector<std::string>&& args)
{
    GetPLDMTypes().exec(args);
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

    // Insert the PLDM message type and EID at the begining of the request
    // msg.
    requestMsg.insert(requestMsg.begin(), MCTP_MSG_TYPE_PLDM);
    requestMsg.insert(requestMsg.begin(), PLDM_ENTITY_ID);

    cout << "Request Message:" << endl;
    printBuffer(requestMsg);

    // Create the response message
    vector<uint8_t> responseMsg;

    // Compares the response with request packet on first socket recv()
    // call. If above condition is qualified then, reads the actual response
    // from the socket to output buffer responseMsg.
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

/*
 * Main function that handles the PLDM raw command response callback via
 * mctp
 *
 */
void handleRawOp(vector<std::string>&& args)
{
    RawOp().exec(args);
}
