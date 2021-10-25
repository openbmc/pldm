#include "pldm_fw_update_cmd.hpp"

#include "libpldm/firmware_update.h"

#include "common/utils.hpp"
#include "pldm_cmd_helper.hpp"

namespace pldmtool
{

namespace fw_update
{

namespace
{

using namespace pldmtool::helper;

std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

const std::map<uint8_t, std::string> fdStateMachine{
    {PLDM_FD_STATE_IDLE, "IDLE"},
    {PLDM_FD_STATE_LEARN_COMPONENTS, "LEARN COMPONENTS"},
    {PLDM_FD_STATE_READY_XFER, "READY XFER"},
    {PLDM_FD_STATE_DOWNLOAD, "DOWNLOAD"},
    {PLDM_FD_STATE_VERIFY, "VERIFY"},
    {PLDM_FD_STATE_APPLY, "APPLY"},
    {PLDM_FD_STATE_ACTIVATE, "ACTIVATE"}};

const std::map<uint8_t, const char*> fdAuxState{
    {PLDM_FD_OPERATION_IN_PROGRESS, "Operation in progress"},
    {PLDM_FD_OPERATION_SUCCESSFUL, "Operation successful"},
    {PLDM_FD_OPERATION_FAILED, "Operation Failed"},
    {PLDM_FD_IDLE_LEARN_COMPONENTS_READ_XFER,
     "Not applicable in current state"}};

const std::map<uint8_t, const char*> fdAuxStateStatus{
    {PLDM_FD_AUX_STATE_IN_PROGRESS_OR_SUCCESS,
     "AuxState is In Progress or Success"},
    {PLDM_FD_TIMEOUT, "Timeout occurred while performing action"},
    {PLDM_FD_GENERIC_ERROR, "Generic Error has occured"}};

const std::map<uint8_t, const char*> fdReasonCode{
    {PLDM_FD_INITIALIZATION, "Initialization of firmware device has occurred"},
    {PLDM_FD_ACTIVATE_FW, "ActivateFirmware command was received"},
    {PLDM_FD_CANCEL_UPDATE, "CancelUpdate command was received"},
    {PLDM_FD_TIMEOUT_LEARN_COMPONENT,
     "Timeout occurred when in LEARN COMPONENT state"},
    {PLDM_FD_TIMEOUT_READY_XFER, "Timeout occurred when in READY XFER state"},
    {PLDM_FD_TIMEOUT_DOWNLOAD, "Timeout occurred when in DOWNLOAD state"},
    {PLDM_FD_TIMEOUT_VERIFY, "Timeout occurred when in VERIFY state"},
    {PLDM_FD_TIMEOUT_APPLY, "Timeout occurred when in APPLY state"}};

class GetStatus : public CommandInterface
{
  public:
    ~GetStatus() = default;
    GetStatus() = delete;
    GetStatus(const GetStatus&) = delete;
    GetStatus(GetStatus&&) = default;
    GetStatus& operator=(const GetStatus&) = delete;
    GetStatus& operator=(GetStatus&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_STATUS_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        auto rc = encode_get_status_req(instanceId, request,
                                        PLDM_GET_STATUS_REQ_BYTES);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t completionCode = 0;
        uint8_t currentState = 0;
        uint8_t previousState = 0;
        uint8_t auxState = 0;
        uint8_t auxStateStatus = 0;
        uint8_t progressPercent = 0;
        uint8_t reasonCode = 0;
        bitfield32_t updateOptionFlagsEnabled{0};

        auto rc = decode_get_status_resp(
            responsePtr, payloadLength, &completionCode, &currentState,
            &previousState, &auxState, &auxStateStatus, &progressPercent,
            &reasonCode, &updateOptionFlagsEnabled);
        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)completionCode << "\n";
            return;
        }

        ordered_json data;
        data["CurrentState"] = fdStateMachine.at(currentState);
        data["PreviousState"] = fdStateMachine.at(previousState);
        data["AuxState"] = fdAuxState.at(auxState);
        if (auxStateStatus >= PLDM_FD_VENDOR_DEFINED_STATUS_CODE_START &&
            auxStateStatus <= PLDM_FD_VENDOR_DEFINED_STATUS_CODE_END)
        {
            data["AuxStateStatus"] = auxStateStatus;
        }
        else
        {
            data["AuxStateStatus"] = fdAuxStateStatus.at(auxStateStatus);
        }
        data["ProgressPercent"] = progressPercent;
        if (reasonCode >= PLDM_FD_STATUS_VENDOR_DEFINED_MIN &&
            reasonCode <= PLDM_FD_STATUS_VENDOR_DEFINED_MAX)
        {
            data["ReasonCode"] = reasonCode;
        }
        else
        {
            data["ReasonCode"] = fdReasonCode.at(reasonCode);
        }
        data["UpdateOptionFlagsEnabled"] = updateOptionFlagsEnabled.value;

        pldmtool::helper::DisplayInJson(data);
    }
};

void registerCommand(CLI::App& app)
{
    auto base = app.add_subcommand("fw_update", "base type command");
    base->require_subcommand(1);

    auto getStatus = base->add_subcommand("GetStatus", "Status of the FD");
    commands.push_back(
        std::make_unique<GetStatus>("fw_update", "GetStatus", getStatus));
}

} // namespace fw_update

} // namespace pldmtool