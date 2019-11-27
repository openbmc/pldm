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

    // The maximum number of record bytes requested to be returned in the
    // response to this instance of the GetPDR command.
    static constexpr uint16_t requestCount = 128;

    explicit GetPDR(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option(
               "-d,--data", recordHandle,
               "retrieve individual PDRs from a PDR Repository\n"
               "eg: The recordHandle value for the PDR to be retrieved and 0 "
               "means get first PDR in the repository.")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_PDR_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_pdr_req(PLDM_LOCAL_INSTANCE_ID, recordHandle, 0,
                                     PLDM_GET_FIRSTPART, requestCount, 0,
                                     request, PLDM_GET_PDR_REQ_BYTES);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t completionCode = 0;
        uint8_t recordData[255] = {0};
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

        printPDRMsg(nextRecordHndl, respCnt, recordData, sizeof(recordData));
    }

  private:
    void printPDR11(uint8_t* data, size_t len)
    {
        if (data == NULL || len == 0)
        {
            return;
        }

        struct pldm_state_effecter_pdr* pdr =
            (struct pldm_state_effecter_pdr*)data;
        std::cout << "recordHandle: " << pdr->hdr.record_handle << std::endl;
        std::cout << "PDRHeaderVersion: " << unsigned(pdr->hdr.version)
                  << std::endl;
        std::cout << "PDRType: " << unsigned(pdr->hdr.type) << std::endl;
        std::cout << "recordChangeNumber: " << pdr->hdr.record_change_num
                  << std::endl;
        std::cout << "dataLength: " << pdr->hdr.length << std::endl;
        std::cout << "PLDMTerminusHandle: " << pdr->terminus_handle
                  << std::endl;
        std::cout << "effecterID: " << pdr->effecter_id << std::endl;
        std::cout << "entityType: " << pdr->entity_type << std::endl;
        std::cout << "entityInstanceNumber: " << pdr->entity_instance
                  << std::endl;
        std::cout << "containerID: " << pdr->container_id << std::endl;
        std::cout << "effecterSemanticID: " << pdr->effecter_semantic_id
                  << std::endl;
        std::cout << "effecterInit: " << unsigned(pdr->effecter_init)
                  << std::endl;
        std::cout << "effecterDescriptionPDR: "
                  << (unsigned(pdr->has_description_pdr) ? "true" : "false")
                  << std::endl;
        std::cout << "compositeEffecterCount: "
                  << unsigned(pdr->composite_effecter_count) << std::endl;

        for (size_t i = 0; i < pdr->composite_effecter_count; ++i)
        {
            struct state_effecter_possible_states* state =
                (struct state_effecter_possible_states*)pdr->possible_states +
                i * sizeof(state_effecter_possible_states);
            std::cout << "stateSetID: " << state->state_set_id << std::endl;
            std::cout << "possibleStatesSize: "
                      << unsigned(state->possible_states_size) << std::endl;
            bitfield8_t* bf = reinterpret_cast<bitfield8_t*>(state->states);
            std::cout << "possibleStates: " << unsigned(bf->byte) << std::endl;
        }
    }

    void printPDRMsg(const uint32_t nextRecordHndl, const uint16_t respCnt,
                     uint8_t* data, size_t len)
    {
        if (data == NULL || len == 0)
        {
            return;
        }

        std::cout << "Parsed Response Msg: " << std::endl;
        std::cout << "nextRecordHandle: " << nextRecordHndl << std::endl;
        std::cout << "responseCount: " << respCnt << std::endl;

        struct pldm_pdr_hdr* pdr = (struct pldm_pdr_hdr*)data;
        switch (pdr->type)
        {
            case PLDM_STATE_EFFECTER_PDR:
                printPDR11(data, len);
                break;
            default:
                break;
        }
    }

  private:
    uint32_t recordHandle;
};

class SetStateEffecter : public CommandInterface
{
  public:
    ~SetStateEffecter() = default;
    SetStateEffecter() = delete;
    SetStateEffecter(const SetStateEffecter&) = delete;
    SetStateEffecter(SetStateEffecter&&) = default;
    SetStateEffecter& operator=(const SetStateEffecter&) = delete;
    SetStateEffecter& operator=(SetStateEffecter&&) = default;

    //  effecterID(1) +  compositeEffecterCount(value: 0x01 to 0x08) *
    //  stateField(2)
    static constexpr auto maxEffecterDataSize = 17;
    explicit SetStateEffecter(const char* type, const char* name,
                              CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option(
               "-d,--data", effecterData,
               "set effecter state data, the noChange value is 0 and the "
               "requestSet value is 1 and access up to eight sets of state "
               "effector information. \n"
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

        if (effecterData.size() > maxEffecterDataSize)
        {
            std::cerr << "Request Message Error: effecterData size "
                      << effecterData.size() << std::endl;
            auto rc = PLDM_ERROR_INVALID_DATA;
            return {rc, requestMsg};
        }

        uint16_t effecterId = 0;
        std::vector<set_effecter_state_field> stateField = {};
        if (!decodeEffecterData(effecterData, effecterId, stateField))
        {
            auto rc = PLDM_ERROR_INVALID_DATA;
            return {rc, requestMsg};
        }

        auto rc = encode_set_state_effecter_states_req(
            PLDM_LOCAL_INSTANCE_ID, effecterId, stateField.size(),
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

        std::cout << "SetStateEffecterStates: SUCCESS" << std::endl;
    }

  private:
    std::vector<uint8_t> effecterData;
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
    commands.push_back(std::make_unique<SetStateEffecter>(
        "platform", "setStateEffecterStates", setStateEffecterStates));
}

} // namespace platform
} // namespace pldmtool
