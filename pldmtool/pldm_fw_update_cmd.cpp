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

const std::map<uint16_t, std::string> componentClassification{
    {PLDM_COMP_UNKNOWN, "Unknown"},
    {PLDM_COMP_OTHER, "Other"},
    {PLDM_COMP_DRIVER, "Driver"},
    {PLDM_COMP_CONFIGURATION_SOFTWARE, "Configuration Software"},
    {PLDM_COMP_APPLICATION_SOFTWARE, "Application Software"},
    {PLDM_COMP_INSTRUMENTATION, "Instrumentation"},
    {PLDM_COMP_FIRMWARE_OR_BIOS, "Firmware/BIOS"},
    {PLDM_COMP_DIAGNOSTIC_SOFTWARE, "Diagnostic Software"},
    {PLDM_COMP_OPERATING_SYSTEM, "Operating System"},
    {PLDM_COMP_MIDDLEWARE, "Middleware"},
    {PLDM_COMP_FIRMWARE, "Firmware"},
    {PLDM_COMP_BIOS_OR_FCODE, "BIOS/FCode"},
    {PLDM_COMP_SUPPORT_OR_SERVICEPACK, "Support/Service Pack"},
    {PLDM_COMP_SOFTWARE_BUNDLE, "Software Bundle"},
    {PLDM_COMP_DOWNSTREAM_DEVICE, "Downstream Device"}};

class GetFwParams : public CommandInterface
{
  public:
    ~GetFwParams() = default;
    GetFwParams() = delete;
    GetFwParams(const GetFwParams&) = delete;
    GetFwParams(GetFwParams&&) = default;
    GetFwParams& operator=(const GetFwParams&) = delete;
    GetFwParams& operator=(GetFwParams&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        auto rc = encode_get_firmware_parameters_req(
            instanceId, PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        pldm_get_firmware_parameters_resp fwParams{};
        variable_field activeCompImageSetVersion{};
        variable_field pendingCompImageSetVersion{};
        variable_field compParameterTable{};

        auto rc = decode_get_firmware_parameters_resp(
            responsePtr, payloadLength, &fwParams, &activeCompImageSetVersion,
            &pendingCompImageSetVersion, &compParameterTable);
        if (rc != PLDM_SUCCESS || fwParams.completion_code != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)fwParams.completion_code
                      << "\n";
            return;
        }

        ordered_json capabilitiesDuringUpdate;
        if (fwParams.capabilities_during_update.bits.bit0)
        {
            capabilitiesDuringUpdate
                ["Component Update Failure Recovery Capability"] =
                    "Device will not revert to previous component image upon failure, timeout or cancellation of the transfer.";
        }
        else
        {
            capabilitiesDuringUpdate
                ["Component Update Failure Recovery Capability"] =
                    "Device will revert to previous component image upon failure, timeout or cancellation of the transfer.";
        }

        if (fwParams.capabilities_during_update.bits.bit1)
        {
            capabilitiesDuringUpdate["Component Update Failure Retry Capability"] =
                "Device will not be able to update component again unless it exits update mode and the UA sends a new Request Update command.";
        }
        else
        {
            capabilitiesDuringUpdate["Component Update Failure Retry Capability"] =
                " Device can have component updated again without exiting update mode and restarting transfer via RequestUpdate command.";
        }

        if (fwParams.capabilities_during_update.bits.bit2)
        {
            capabilitiesDuringUpdate["Firmware Device Partial Updates"] =
                "Firmware Device can support a partial update, whereby a package which contains a component image set that is a subset of all components currently residing on the FD, can be transferred.";
        }
        else
        {
            capabilitiesDuringUpdate["Firmware Device Partial Updates"] =
                "Firmware Device cannot accept a partial update and all components present on the FD shall be updated.";
        }

        if (fwParams.capabilities_during_update.bits.bit3)
        {
            capabilitiesDuringUpdate
                ["Firmware Device Host Functionality during Firmware Update"] =
                    "Device will not revert to previous component image upon failure, timeout or cancellation of the transfer";
        }
        else
        {
            capabilitiesDuringUpdate
                ["Firmware Device Host Functionality during Firmware Update"] =
                    "Device will revert to previous component image upon failure, timeout or cancellation of the transfer";
        }

        if (fwParams.capabilities_during_update.bits.bit4)
        {
            capabilitiesDuringUpdate["Firmware Device Update Mode Restrictions"] =
                "Firmware device unable to enter update mode if host OS environment is active.";
        }
        else
        {
            capabilitiesDuringUpdate
                ["Firmware Device Update Mode Restrictions"] =
                    "No host OS environment restriction for update mode";
        }

        ordered_json data;
        data["CapabilitiesDuringUpdate"] = capabilitiesDuringUpdate;
        data["ComponentCount"] = static_cast<uint16_t>(fwParams.comp_count);
        data["ActiveComponentImageSetVersionString"] =
            pldm::utils::toString(activeCompImageSetVersion);
        data["PendingComponentImageSetVersionString"] =
            pldm::utils::toString(pendingCompImageSetVersion);

        auto compParamPtr = compParameterTable.ptr;
        auto compParamTableLen = compParameterTable.length;
        pldm_component_parameter_entry compEntry{};
        variable_field activeCompVerStr{};
        variable_field pendingCompVerStr{};
        ordered_json compDataEntries;

        while (fwParams.comp_count-- && (compParamTableLen > 0))
        {
            ordered_json compData;
            auto rc = decode_get_firmware_parameters_resp_comp_entry(
                compParamPtr, compParamTableLen, &compEntry, &activeCompVerStr,
                &pendingCompVerStr);
            if (rc)
            {
                std::cerr
                    << "Decoding component parameter table entry failed, RC="
                    << rc << "\n";
                return;
            }

            if (componentClassification.contains(compEntry.comp_classification))
            {
                compData["ComponentClassification"] =
                    componentClassification.at(compEntry.comp_classification);
            }
            else
            {
                compData["ComponentClassification"] =
                    static_cast<uint16_t>(compEntry.comp_classification);
            }
            compData["ComponentIdentifier"] =
                static_cast<uint16_t>(compEntry.comp_identifier);
            compData["ComponentClassificationIndex"] =
                static_cast<uint8_t>(compEntry.comp_classification_index);
            compData["ActiveComponentComparisonStamp"] =
                static_cast<uint32_t>(compEntry.active_comp_comparison_stamp);

            // ActiveComponentReleaseData
            std::array<uint8_t, 8> noReleaseData{0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x00};
            if (std::equal(noReleaseData.begin(), noReleaseData.end(),
                           compEntry.active_comp_release_date))
            {
                compData["ActiveComponentReleaseDate"] = "";
            }
            else
            {
                std::string activeComponentReleaseDate(
                    reinterpret_cast<const char*>(
                        compEntry.active_comp_release_date),
                    sizeof(compEntry.active_comp_release_date));
                compData["ActiveComponentReleaseDate"] =
                    activeComponentReleaseDate;
            }

            compData["PendingComponentComparisonStamp"] =
                static_cast<uint32_t>(compEntry.pending_comp_comparison_stamp);

            // PendingComponentReleaseData
            if (std::equal(noReleaseData.begin(), noReleaseData.end(),
                           compEntry.pending_comp_release_date))
            {
                compData["PendingComponentReleaseDate"] = "";
            }
            else
            {
                std::string pendingComponentReleaseDate(
                    reinterpret_cast<const char*>(
                        compEntry.pending_comp_release_date),
                    sizeof(compEntry.pending_comp_release_date));
                compData["PendingComponentReleaseDate"] =
                    pendingComponentReleaseDate;
            }

            // ComponentActivationMethods
            ordered_json componentActivationMethods;
            if (compEntry.comp_activation_methods.bits.bit0)
            {
                componentActivationMethods.push_back("Automatic");
            }
            else if (compEntry.comp_activation_methods.bits.bit1)
            {
                componentActivationMethods.push_back("Self-Contained");
            }
            else if (compEntry.comp_activation_methods.bits.bit2)
            {
                componentActivationMethods.push_back("Medium-specific reset");
            }
            else if (compEntry.comp_activation_methods.bits.bit3)
            {
                componentActivationMethods.push_back("System reboot");
            }
            else if (compEntry.comp_activation_methods.bits.bit4)
            {
                componentActivationMethods.push_back("DC power cycel");
            }
            else if (compEntry.comp_activation_methods.bits.bit5)
            {
                componentActivationMethods.push_back("AC power cycle");
            }
            compData["ComponentActivationMethods"] = componentActivationMethods;

            // CapabilitiesDuringUpdate
            ordered_json compCapabilitiesDuringUpdate;
            if (compEntry.capabilities_during_update.bits.bit0)
            {
                compCapabilitiesDuringUpdate
                    ["Firmware Device apply state functionality"] =
                        "Firmware Device performs an auto-apply during transfer phase and apply step will be completed immediately.";
            }
            else
            {
                compCapabilitiesDuringUpdate
                    ["Firmware Device apply state functionality"] =
                        " Firmware Device will execute an operation during the APPLY state which will include migrating the new component image to its final non-volatile storage destination.";
            }
            compData["CapabilitiesDuringUpdate"] = compCapabilitiesDuringUpdate;

            compData["ActiveComponentVersionString"] =
                pldm::utils::toString(activeCompVerStr);
            compData["PendingComponentVersionString"] =
                pldm::utils::toString(pendingCompVerStr);

            compParamPtr += sizeof(pldm_component_parameter_entry) +
                            activeCompVerStr.length + pendingCompVerStr.length;
            compParamTableLen -= sizeof(pldm_component_parameter_entry) +
                                 activeCompVerStr.length +
                                 pendingCompVerStr.length;
            compDataEntries.push_back(compData);
        }
        data["ComponentParameterEntries"] = compDataEntries;

        pldmtool::helper::DisplayInJson(data);
    }
};

void registerCommand(CLI::App& app)
{
    auto fwUpdate =
        app.add_subcommand("fw_update", "firmware update type commands");
    fwUpdate->require_subcommand(1);

    auto getStatus = fwUpdate->add_subcommand("GetStatus", "Status of the FD");
    commands.push_back(
        std::make_unique<GetStatus>("fw_update", "GetStatus", getStatus));

    auto getFwParams = fwUpdate->add_subcommand(
        "GetFwParams", "To get the component details of the FD");
    commands.push_back(
        std::make_unique<GetFwParams>("fw_update", "GetFwParams", getFwParams));
}

} // namespace fw_update

} // namespace pldmtool