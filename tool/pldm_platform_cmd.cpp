#include "pldm_cmd_helper.hpp"

#include <string>
#include <vector>

namespace pldmtool
{

namespace platform
{

namespace
{

using namespace pldmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

class GetPDR : public CommandInterface
{
  public:
    ~GetPDR() = default;
    GetPDR() = delete;
    GetPDR(const GetPDR&) = delete;
    GetPDR(GetPDR&&) = default;
    GetPDR& operator=(const GetPDR&) = delete;
    GetPDR& operator=(GetPDR&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_PDR_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        uint16_t request_cnt = 128;
        auto rc =
            encode_get_pdr_req(PLDM_LOCAL_INSTANCE_ID, 0, 0, PLDM_GET_FIRSTPART,
                               request_cnt, 0, request, PLDM_GET_PDR_REQ_BYTES);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t completionCode = 0;
        uint8_t recordData[32] = {0};
        uint32_t nextRecordHndl = 0;
        uint32_t nextDataTransferHndl = 0;
        uint8_t transferFlag = 0;
        uint16_t respCnt = 0;
        uint8_t transferCRC = 0;

        auto rc = decode_get_pdr_resp(
            responsePtr, payloadLength, &completionCode, &nextRecordHndl,
            &nextDataTransferHndl, &transferFlag, &respCnt, recordData,
            sizeof(recordData), &transferCRC);

        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)completionCode
                      << std::endl;
            return;
        }
    }
};

class SetStateEffecterStates : public CommandInterface
{
  public:
    ~SetStateEffecterStates() = default;
    SetStateEffecterStates() = delete;
    SetStateEffecterStates(const SetStateEffecterStates&) = delete;
    SetStateEffecterStates(SetStateEffecterStates&&) = default;
    SetStateEffecterStates& operator=(const SetStateEffecterStates&) = delete;
    SetStateEffecterStates& operator=(SetStateEffecterStates&&) = default;

    explicit SetStateEffecterStates(const char* type, const char* name,
                                    CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option(
               "-d,--data", effData,
               "set effecter state data \n"
               "eg1: effecterID, requestSet0, effecterState0... \n"
               "eg2: effecterID, noChange0, requestSet1, effecterState1...")
            ->required()
            ->expected(-1);
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        if (effData.size() > 17)
        {
            //  effecterID(1) +  compositeEffecterCount(value: 0x01 to 0x08) *
            //  stateField(2)
            std::cerr << "Request Message Error: effData size "
                      << effData.size() << std::endl;
            auto rc = PLDM_ERROR_INVALID_DATA;
            return {rc, requestMsg};
        }

        uint16_t effecter_id = 0;
        std::vector<set_effecter_state_field> stateField;
        int flag = 0;
        for (auto data : effData)
        {
            switch (flag)
            {
                case 0:
                    effecter_id = data;
                    flag = 1;
                    break;
                case 1:
                    if (data == PLDM_REQUEST_SET)
                    {
                        flag = 2;
                    }
                    else
                    {
                        stateField.push_back({PLDM_NO_CHANGE, 0});
                    }
                    break;
                case 2:
                    stateField.push_back({PLDM_REQUEST_SET, data});
                    flag = 1;
                    break;
                default:
                    break;
            }
        }

        auto rc = encode_set_state_effecter_states_req(
            PLDM_LOCAL_INSTANCE_ID, effecter_id, stateField.size(),
            stateField.data(), request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t completionCode = 0;
        auto rc = decode_set_state_effecter_states_resp(
            responsePtr, payloadLength, &completionCode);

        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)completionCode
                      << std::endl;
            return;
        }
    }

  private:
    std::vector<uint8_t> effData;
};

void registerCommand(CLI::App& app)
{
    auto platform = app.add_subcommand("platform", "platform type command");
    platform->require_subcommand(1);

    auto getPDR =
        platform->add_subcommand("GetPDR", "get platform descriptor records");
    commands.push_back(std::make_unique<GetPDR>("platform", "getPDR", getPDR));

    auto setStateEffecterStates = platform->add_subcommand(
        "SetStateEffecterStates", "set effecter states");
    commands.push_back(std::make_unique<SetStateEffecterStates>(
        "platform", "setStateEffecterStates", setStateEffecterStates));
}

} // namespace platform
} // namespace pldmtool
