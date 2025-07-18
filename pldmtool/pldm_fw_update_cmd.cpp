#include "pldm_fw_update_cmd.hpp"

#include "common/utils.hpp"
#include "pldm_cmd_helper.hpp"

#include <libpldm/firmware_update.h>

#include <format>

namespace pldmtool
{

namespace fw_update
{

namespace
{

using namespace pldmtool::helper;
using namespace pldm::fw_update;

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
    {PLDM_FD_GENERIC_ERROR, "Generic Error has occurred"}};

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

/**
 * @brief descriptor type to name mapping
 *
 */
const std::map<DescriptorType, const char*> descriptorName{
    {PLDM_FWUP_PCI_VENDOR_ID, "PCI Vendor ID"},
    {PLDM_FWUP_IANA_ENTERPRISE_ID, "IANA Enterprise ID"},
    {PLDM_FWUP_UUID, "UUID"},
    {PLDM_FWUP_PNP_VENDOR_ID, "PnP Vendor ID"},
    {PLDM_FWUP_ACPI_VENDOR_ID, "ACPI Vendor ID"},
    {PLDM_FWUP_PCI_DEVICE_ID, "PCI Device ID"},
    {PLDM_FWUP_PCI_SUBSYSTEM_VENDOR_ID, "PCI Subsystem Vendor ID"},
    {PLDM_FWUP_PCI_SUBSYSTEM_ID, "PCI Subsystem ID"},
    {PLDM_FWUP_PCI_REVISION_ID, "PCI Revision ID"},
    {PLDM_FWUP_PNP_PRODUCT_IDENTIFIER, "PnP Product Identifier"},
    {PLDM_FWUP_ACPI_PRODUCT_IDENTIFIER, "ACPI Product Identifier"},
    {PLDM_FWUP_VENDOR_DEFINED, "Vendor Defined"}};

const std::map<std::string, transfer_resp_flag> transferRespFlag{
    {"START", PLDM_START},
    {"MIDDLE", PLDM_MIDDLE},
    {"END", PLDM_END},
    {"STARTANDEND", PLDM_START_AND_END},
};

const std::map<const char*, pldm_self_contained_activation_req>
    pldmSelfContainedActivation{
        {"False", PLDM_NOT_ACTIVATE_SELF_CONTAINED_COMPONENTS},
        {"True", PLDM_ACTIVATE_SELF_CONTAINED_COMPONENTS},
    };

/*
 * Convert PLDM Firmware String Type to uint8_t
 *
 *  @param[in]  compImgVerStrType - the component version string
 *
 *  @return - the component version string converted to a numeric value.
 */
uint8_t convertStringTypeToUInt8(std::string compImgVerStrType)
{
    static const std::map<std::string, pldm_firmware_update_string_type>
        pldmFirmwareUpdateStringType{
            {"UNKNOWN", PLDM_STR_TYPE_UNKNOWN},
            {"ASCII", PLDM_STR_TYPE_ASCII},
            {"UTF_8", PLDM_STR_TYPE_UTF_8},
            {"UTF_16", PLDM_STR_TYPE_UTF_16},
            {"UTF_16LE", PLDM_STR_TYPE_UTF_16LE},
            {"UTF_16BE", PLDM_STR_TYPE_UTF_16BE},
        };

    if (pldmFirmwareUpdateStringType.contains(compImgVerStrType))
    {
        return pldmFirmwareUpdateStringType.at(compImgVerStrType);
    }
    else
    {
        return static_cast<uint8_t>(std::stoi(compImgVerStrType));
    }
}

class GetStatus : public CommandInterface
{
  public:
    ~GetStatus() = default;
    GetStatus() = delete;
    GetStatus(const GetStatus&) = delete;
    GetStatus(GetStatus&&) = default;
    GetStatus& operator=(const GetStatus&) = delete;
    GetStatus& operator=(GetStatus&&) = delete;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + PLDM_GET_STATUS_REQ_BYTES);
        auto request = new (requestMsg.data()) pldm_msg;
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
    GetFwParams& operator=(GetFwParams&&) = delete;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
        auto request = new (requestMsg.data()) pldm_msg;
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

        if (fwParams.capabilities_during_update.bits.bit3)
        {
            capabilitiesDuringUpdate["Firmware Device Partial Updates"] =
                "Firmware Device can support a partial update, whereby a package which contains a component image set that is a subset of all components currently residing on the FD, can be transferred.";
        }
        else
        {
            capabilitiesDuringUpdate["Firmware Device Partial Updates"] =
                "Firmware Device cannot accept a partial update and all components present on the FD shall be updated.";
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
            compParamTableLen -=
                sizeof(pldm_component_parameter_entry) +
                activeCompVerStr.length + pendingCompVerStr.length;
            compDataEntries.push_back(compData);
        }
        data["ComponentParameterEntries"] = compDataEntries;

        pldmtool::helper::DisplayInJson(data);
    }
};

class QueryDeviceIdentifiers : public CommandInterface
{
  public:
    ~QueryDeviceIdentifiers() = default;
    QueryDeviceIdentifiers() = delete;
    QueryDeviceIdentifiers(const QueryDeviceIdentifiers&) = delete;
    QueryDeviceIdentifiers(QueryDeviceIdentifiers&&) = default;
    QueryDeviceIdentifiers& operator=(const QueryDeviceIdentifiers&) = delete;
    QueryDeviceIdentifiers& operator=(QueryDeviceIdentifiers&&) = delete;

    /**
     * @brief Implementation of createRequestMsg for QueryDeviceIdentifiers
     *
     * @return std::pair<int, std::vector<uint8_t>>
     */
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override;

    /**
     * @brief Implementation of parseResponseMsg for QueryDeviceIdentifiers
     *
     * @param[in] responsePtr
     * @param[in] payloadLength
     */
    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override;
    using CommandInterface::CommandInterface;

  private:
    /**
     * @brief Method to update QueryDeviceIdentifiers json response in a user
     * friendly format
     *
     * @param[in] descriptors - descriptor json response
     * @param[in] descriptorType - descriptor type
     * @param[in] descriptorVal - descriptor value
     */
    void updateDescriptor(
        ordered_json& descriptors, const DescriptorType& descriptorType,
        const std::variant<DescriptorData, VendorDefinedDescriptorInfo>&
            descriptorVal);
};

void QueryDeviceIdentifiers::updateDescriptor(
    ordered_json& descriptors, const DescriptorType& descriptorType,
    const std::variant<DescriptorData, VendorDefinedDescriptorInfo>&
        descriptorVal)
{
    std::ostringstream descDataStream;
    DescriptorData descData;
    if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
    {
        descData = std::get<DescriptorData>(descriptorVal);
    }
    else
    {
        descData = std::get<VendorDefinedDescriptorData>(
            std::get<VendorDefinedDescriptorInfo>(descriptorVal));
    }
    for (int byte : descData)
    {
        descDataStream << std::setfill('0') << std::setw(2) << std::hex << byte;
    }

    if (descriptorName.contains(descriptorType))
    {
        // Update the existing json response if entry is already present
        for (auto& descriptor : descriptors)
        {
            if (descriptor["Type"] == descriptorName.at(descriptorType))
            {
                if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
                {
                    descriptor["Value"].emplace_back(descDataStream.str());
                }
                else
                {
                    ordered_json vendorDefinedVal;
                    vendorDefinedVal[std::get<VendorDefinedDescriptorTitle>(
                        std::get<VendorDefinedDescriptorInfo>(descriptorVal))] =
                        descDataStream.str();
                    descriptor["Value"].emplace_back(vendorDefinedVal);
                }
                return;
            }
        }
        // Entry is not present, add type and value to json response
        ordered_json descriptor = ordered_json::object(
            {{"Type", descriptorName.at(descriptorType)},
             {"Value", ordered_json::array()}});
        if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
        {
            descriptor["Value"].emplace_back(descDataStream.str());
        }
        else
        {
            ordered_json vendorDefinedVal;
            vendorDefinedVal[std::get<VendorDefinedDescriptorTitle>(
                std::get<VendorDefinedDescriptorInfo>(descriptorVal))] =
                descDataStream.str();
            descriptor["Value"].emplace_back(vendorDefinedVal);
        }
        descriptors.emplace_back(descriptor);
    }
    else
    {
        std::cerr << "Unknown descriptor type, type=" << descriptorType << "\n";
    }
}
std::pair<int, std::vector<uint8_t>> QueryDeviceIdentifiers::createRequestMsg()
{
    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES);
    auto request = new (requestMsg.data()) pldm_msg;
    auto rc = encode_query_device_identifiers_req(
        instanceId, PLDM_QUERY_DEVICE_IDENTIFIERS_REQ_BYTES, request);
    return {rc, requestMsg};
}

void QueryDeviceIdentifiers::parseResponseMsg(pldm_msg* responsePtr,
                                              size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    uint32_t deviceIdentifiersLen = 0;
    uint8_t descriptorCount = 0;
    uint8_t* descriptorPtr = nullptr;
    uint8_t eid = getMCTPEID();
    auto rc = decode_query_device_identifiers_resp(
        responsePtr, payloadLength, &completionCode, &deviceIdentifiersLen,
        &descriptorCount, &descriptorPtr);
    if (rc)
    {
        std::cerr << "Decoding QueryDeviceIdentifiers response failed,EID="
                  << unsigned(eid) << ", RC=" << rc << "\n";
        return;
    }
    if (completionCode)
    {
        std::cerr << "QueryDeviceIdentifiers response failed with error "
                     "completion code, EID="
                  << unsigned(eid) << ", CC=" << unsigned(completionCode)
                  << "\n";
        return;
    }
    ordered_json data;
    data["EID"] = eid;
    ordered_json descriptors;
    while (descriptorCount-- && (deviceIdentifiersLen > 0))
    {
        DescriptorType descriptorType = 0;
        variable_field descriptorData{};

        rc = decode_descriptor_type_length_value(
            descriptorPtr, deviceIdentifiersLen, &descriptorType,
            &descriptorData);
        if (rc)
        {
            std::cerr << "Decoding descriptor type, length and value failed,"
                      << "EID=" << unsigned(eid) << ",RC=" << rc << "\n ";
            return;
        }

        if (descriptorType != PLDM_FWUP_VENDOR_DEFINED)
        {
            std::vector<uint8_t> descData(
                descriptorData.ptr, descriptorData.ptr + descriptorData.length);
            updateDescriptor(descriptors, descriptorType, descData);
        }
        else
        {
            uint8_t descriptorTitleStrType = 0;
            variable_field descriptorTitleStr{};
            variable_field vendorDefinedDescriptorData{};

            rc = decode_vendor_defined_descriptor_value(
                descriptorData.ptr, descriptorData.length,
                &descriptorTitleStrType, &descriptorTitleStr,
                &vendorDefinedDescriptorData);
            if (rc)
            {
                std::cerr << "Decoding Vendor-defined descriptor value"
                          << "failed EID=" << unsigned(eid) << ", RC=" << rc
                          << "\n ";
                return;
            }

            auto vendorDescTitle = pldm::utils::toString(descriptorTitleStr);
            std::vector<uint8_t> vendorDescData(
                vendorDefinedDescriptorData.ptr,
                vendorDefinedDescriptorData.ptr +
                    vendorDefinedDescriptorData.length);
            updateDescriptor(descriptors, descriptorType,
                             std::make_tuple(vendorDescTitle, vendorDescData));
        }
        auto nextDescriptorOffset =
            sizeof(pldm_descriptor_tlv().descriptor_type) +
            sizeof(pldm_descriptor_tlv().descriptor_length) +
            descriptorData.length;
        descriptorPtr += nextDescriptorOffset;
        deviceIdentifiersLen -= nextDescriptorOffset;
    }
    data["Descriptors"] = descriptors;
    pldmtool::helper::DisplayInJson(data);
}

class RequestUpdate : public CommandInterface
{
  public:
    ~RequestUpdate() = default;
    RequestUpdate() = delete;
    RequestUpdate(const RequestUpdate&) = delete;
    RequestUpdate(RequestUpdate&&) = delete;
    RequestUpdate& operator=(const RequestUpdate&) = delete;
    RequestUpdate& operator=(RequestUpdate&&) = delete;

    explicit RequestUpdate(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option(
               "--max_transfer_size", maxTransferSize,
               "Specifies the maximum size, in bytes, of the variable payload allowed to\n"
               "be requested by the FD via the RequestFirmwareData command that is contained\n"
               "within a PLDM message. This value shall be equal to or greater than firmware update\n"
               "baseline transfer size.")
            ->required();

        app->add_option(
               "--num_comps", numComps,
               "Specifies the number of components that will be passed to the FD during the update.\n"
               "The FD can use this value to compare against the number of PassComponentTable\n"
               "commands received.")
            ->required();

        app->add_option(
               "--max_transfer_reqs", maxTransferReqs,
               "Specifies the number of outstanding RequestFirmwareData commands that can be\n"
               "sent by the FD. The minimum required value is '1' which the UA shall support.\n"
               "It is optional for the UA to support a value higher than '1' for this field.")
            ->required();

        app->add_option(
               "--package_data_length", packageDataLength,
               "This field shall be set to the value contained within\n"
               "the FirmwareDevicePackageDataLength field that was provided in\n"
               "the firmware package header. If no firmware package data was\n"
               "provided in the firmware update package then this length field\n"
               "shall be set to 0x0000.")
            ->required();

        app->add_option(
               "--comp_img_ver_str_type", compImgVerStrType,
               "The type of string used in the ComponentImageSetVersionString\n"
               "field. Possible values\n"
               "{UNKNOWN->0, ASCII->1, UTF_8->2, UTF_16->3, UTF_16LE->4, UTF_16BE->5}\n"
               "OR {0,1,2,3,4,5}")
            ->required()
            ->check([](const std::string& value) -> std::string {
                static const std::set<std::string> validStrings{
                    "UNKNOWN", "ASCII",    "UTF_8",
                    "UTF_16",  "UTF_16LE", "UTF_16BE"};

                if (validStrings.contains(value))
                {
                    return "";
                }

                try
                {
                    int intValue = std::stoi(value);
                    if (intValue >= 0 && intValue <= 255)
                    {
                        return "";
                    }
                    return "Invalid value. Must be one of UNKNOWN, ASCII, UTF_8, UTF_16, UTF_16LE, UTF_16BE, or a number between 0 and 255";
                }
                catch (const std::exception&)
                {
                    return "Invalid value. Must be one of UNKNOWN, ASCII, UTF_8, UTF_16, UTF_16LE, UTF_16BE, or a number between 0 and 255";
                }
            });

        app->add_option(
               "--comp_img_ver_str_len", compImgVerStrLen,
               "The length, in bytes, of the ComponentImageSetVersionString.")
            ->required();

        app->add_option(
               "--comp_img_set_ver_str", compImgSetVerStr,
               "Component Image Set version information, up to 255 bytes.")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        variable_field compImgSetVerStrInfo{};
        compImgSetVerStrInfo.ptr =
            reinterpret_cast<const uint8_t*>(compImgSetVerStr.data());
        compImgSetVerStrInfo.length =
            static_cast<uint8_t>(compImgSetVerStr.size());

        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + sizeof(struct pldm_request_update_req) +
            compImgSetVerStrInfo.length);

        auto request = new (requestMsg.data()) pldm_msg;

        auto rc = encode_request_update_req(
            instanceId, maxTransferSize, numComps, maxTransferReqs,
            packageDataLength, convertStringTypeToUInt8(compImgVerStrType),
            compImgVerStrLen, &compImgSetVerStrInfo, request,
            sizeof(struct pldm_request_update_req) +
                compImgSetVerStrInfo.length);

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint16_t fdMetaDataLen = 0;
        uint8_t fdWillSendPkgData = 0;

        auto rc =
            decode_request_update_resp(responsePtr, payloadLength, &cc,
                                       &fdMetaDataLen, &fdWillSendPkgData);
        if (rc)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << "\n";
            return;
        }

        ordered_json data;
        fillCompletionCode(cc, data, PLDM_FWUP);

        if (cc == PLDM_SUCCESS)
        {
            data["FirmwareDeviceMetaDataLength"] = fdMetaDataLen;
            data["FDWillSendGetPackageDataCommand"] =
                std::format("0x{:02X}", fdWillSendPkgData);
        }
        pldmtool::helper::DisplayInJson(data);
    }

  private:
    uint32_t maxTransferSize;
    uint16_t numComps;
    uint8_t maxTransferReqs;
    uint16_t packageDataLength;
    std::string compImgVerStrType;
    uint8_t compImgVerStrLen;
    std::string compImgSetVerStr;
};

class PassComponentTable : public CommandInterface
{
  public:
    ~PassComponentTable() = default;
    PassComponentTable() = delete;
    PassComponentTable(const PassComponentTable&) = delete;
    PassComponentTable(PassComponentTable&&) = delete;
    PassComponentTable& operator=(const PassComponentTable&) = delete;
    PassComponentTable& operator=(PassComponentTable&&) = delete;

    explicit PassComponentTable(const char* type, const char* name,
                                CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option(
               "--transfer_flag", transferFlag,
               "The transfer flag that indicates what part of the Component Table\n"
               "this request represents.\nPossible values\n"
               "{Start = 0x1, Middle = 0x2, End = 0x4, StartAndEnd = 0x5}")
            ->required()
            ->transform(
                CLI::CheckedTransformer(transferRespFlag, CLI::ignore_case));

        app->add_option(
               "--comp_classification", compClassification,
               "Vendor specific component classification information.\n"
               "Special values: 0x0000, 0xFFFF = reserved")
            ->required();

        app->add_option("--comp_identifier", compIdentifier,
                        "FD vendor selected unique value "
                        "to distinguish between component images")
            ->required();

        app->add_option("--comp_classification_idx", compClassificationIdx,
                        "The component classification index which was obtained "
                        "from the GetFirmwareParameters command to indicate \n"
                        "which firmware component the information contained "
                        "within this command is applicable for")
            ->required();

        app->add_option(
               "--comp_compare_stamp", compCompareStamp,
               "FD vendor selected value to use as a comparison value in determining if a firmware\n"
               "component is down-level or up-level. For the same component identifier,\n"
               "the greater of two component comparison stamps is considered up-level compared\n"
               "to the other when performing an unsigned integer comparison")
            ->required();

        app->add_option(
               "--comp_ver_str_type", compVerStrType,
               "The type of strings used in the ComponentVersionString\n"
               "Possible values\n"
               "{UNKNOWN->0, ASCII->1, UTF_8->2, UTF_16->3, UTF_16LE->4, UTF_16BE->5}\n"
               "OR {0,1,2,3,4,5}")
            ->required()
            ->check([](const std::string& value) -> std::string {
                static const std::set<std::string> validStrings{
                    "UNKNOWN", "ASCII",    "UTF_8",
                    "UTF_16",  "UTF_16LE", "UTF_16BE"};

                if (validStrings.contains(value))
                {
                    return "";
                }

                try
                {
                    int intValue = std::stoi(value);
                    if (intValue >= 0 && intValue <= 255)
                    {
                        return "";
                    }
                    return "Invalid value. Must be one of UNKNOWN, ASCII, UTF_8, UTF_16, UTF_16LE, UTF_16BE, or a number between 0 and 255";
                }
                catch (const std::exception&)
                {
                    return "Invalid value. Must be one of UNKNOWN, ASCII, UTF_8, UTF_16, UTF_16LE, UTF_16BE, or a number between 0 and 255";
                }
            });

        app->add_option("--comp_ver_str_len", compVerStrLen,
                        "The length, in bytes, of the ComponentVersionString")
            ->required();

        app->add_option(
               "--comp_ver_str", compVerStr,
               "Firmware component version information up to 255 bytes. \n"
               "Contains a variable type string describing the component version")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        variable_field compVerStrInfo{};
        std::vector<uint8_t> compVerStrData(compVerStr.begin(),
                                            compVerStr.end());
        compVerStrInfo.ptr = compVerStrData.data();
        compVerStrInfo.length = static_cast<uint8_t>(compVerStrData.size());

        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) +
            sizeof(struct pldm_pass_component_table_req) +
            compVerStrInfo.length);

        auto request = new (requestMsg.data()) pldm_msg;

        auto rc = encode_pass_component_table_req(
            instanceId, transferFlag, compClassification, compIdentifier,
            compClassificationIdx, compCompareStamp,
            convertStringTypeToUInt8(compVerStrType), compVerStrInfo.length,
            &compVerStrInfo, request,
            sizeof(pldm_pass_component_table_req) + compVerStrInfo.length);

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint8_t compResponse = 0;
        uint8_t compResponseCode = 0;

        auto rc = decode_pass_component_table_resp(
            responsePtr, payloadLength, &cc, &compResponse, &compResponseCode);

        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << "\n";
            return;
        }

        ordered_json data;
        fillCompletionCode(cc, data, PLDM_FWUP);

        if (cc == PLDM_SUCCESS)
        {
            data["ComponentResponse"] =
                compResponse ? "Component may be updateable"
                             : "Component can be updated";

            data["ComponentResponseCode"] =
                std::format("0x{:02X}", compResponseCode);
        }

        pldmtool::helper::DisplayInJson(data);
    }

  private:
    uint8_t transferFlag;
    uint16_t compClassification;
    uint16_t compIdentifier;
    uint8_t compClassificationIdx;
    uint32_t compCompareStamp;
    std::string compVerStrType;
    uint8_t compVerStrLen;
    std::string compVerStr;
};

class UpdateComponent : public CommandInterface
{
  public:
    ~UpdateComponent() = default;
    UpdateComponent() = delete;
    UpdateComponent(const UpdateComponent&) = delete;
    UpdateComponent(UpdateComponent&&) = delete;
    UpdateComponent& operator=(const UpdateComponent&) = delete;
    UpdateComponent& operator=(UpdateComponent&&) = delete;

    explicit UpdateComponent(const char* type, const char* name,
                             CLI::App* app) : CommandInterface(type, name, app)
    {
        app->add_option(
               "--component_classification", componentClassification,
               "Classification value provided by the firmware package header information for\n"
               "the component to be transferred.\n"
               "Special values: 0x0000, 0xFFFF = reserved")
            ->required();

        app->add_option(
               "--component_identifier", componentIdentifier,
               "FD vendor selected unique value to distinguish between component images")
            ->required();

        app->add_option(
               "--component_classification_index", componentClassificationIndex,
               "The component classification index which was obtained from the GetFirmwareParameters\n"
               "command to indicate which firmware component the information contained within this\n"
               "command is applicable for")
            ->required();

        app->add_option(
               "--component_comparison_stamp", componentComparisonStamp,
               "FD vendor selected value to use as a comparison value in determining if a firmware\n"
               "component is down-level or up-level. For the same component identifier, the greater\n"
               "of two component comparison stamps is considered up-level compared to the other\n"
               "when performing an unsigned integer comparison")
            ->required();

        app->add_option("--component_image_size", componentImageSize,
                        "Size in bytes of the component image")
            ->required();

        app->add_option(
               "--update_option_flags", strUpdateOptionFlags,
               "32-bit field, where each non-reserved bit represents an update option that can be\n"
               "requested by the UA to be enabled for the transfer of this component image.\n"
               "[2] Security Revision Number Delayed Update\n"
               "[1] Component Opaque Data\n"
               "[0] Request Force Update of component")
            ->required();

        app->add_option(
               "--component_version_string_type", componentVersionStringType,
               "The type of strings used in the ComponentVersionString\n"
               "Possible values\n"
               "{UNKNOWN->0, ASCII->1, UTF_8->2, UTF_16->3, UTF_16LE->4, UTF_16BE->5}\n"
               "OR {0,1,2,3,4,5}")
            ->required()
            ->check([](const std::string& value) -> std::string {
                static const std::set<std::string> validStrings{
                    "UNKNOWN", "ASCII",    "UTF_8",
                    "UTF_16",  "UTF_16LE", "UTF_16BE"};

                if (validStrings.contains(value))
                {
                    return "";
                }

                try
                {
                    int intValue = std::stoi(value);
                    if (intValue >= 0 && intValue <= 255)
                    {
                        return "";
                    }
                    return "Invalid value. Must be one of UNKNOWN, ASCII, UTF_8, UTF_16, UTF_16LE, UTF_16BE, or a number between 0 and 255";
                }
                catch (const std::exception&)
                {
                    return "Invalid value. Must be one of UNKNOWN, ASCII, UTF_8, UTF_16, UTF_16LE, UTF_16BE, or a number between 0 and 255";
                }
            });

        app->add_option("--component_version_string_length",
                        componentVersionStringLength,
                        "The length, in bytes, of the ComponentVersionString")
            ->required();

        app->add_option(
               "--component_version_string", componentVersionString,
               "Firmware component version information up to 255 bytes.\n"
               "Contains a variable type string describing the component version")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        variable_field componentVersionStringInfo{};

        componentVersionStringInfo.ptr =
            reinterpret_cast<const uint8_t*>(componentVersionString.data());
        componentVersionStringInfo.length =
            static_cast<uint8_t>(componentVersionString.size());

        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + sizeof(struct pldm_update_component_req) +
            componentVersionStringInfo.length);

        auto request = new (requestMsg.data()) pldm_msg;

        bitfield32_t updateOptionFlags;
        std::stringstream ss(strUpdateOptionFlags);
        ss >> std::hex >> updateOptionFlags.value;
        if (ss.fail())
        {
            std::cerr << "Failed to parse update option flags: "
                      << strUpdateOptionFlags << "\n";
            return {PLDM_ERROR_INVALID_DATA, std::vector<uint8_t>()};
        }

        auto rc = encode_update_component_req(
            instanceId, componentClassification, componentIdentifier,
            componentClassificationIndex, componentComparisonStamp,
            componentImageSize, updateOptionFlags,
            convertStringTypeToUInt8(componentVersionStringType),
            componentVersionStringLength, &componentVersionStringInfo, request,
            sizeof(pldm_update_component_req) +
                componentVersionStringInfo.length);

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint8_t componentCompatibilityResp = 0;
        uint8_t componentCompatibilityRespCode = 0;
        bitfield32_t updateOptionFlagsEnabled{};
        uint16_t timeBeforeReqFWData = 0;

        auto rc = decode_update_component_resp(
            responsePtr, payloadLength, &cc, &componentCompatibilityResp,
            &componentCompatibilityRespCode, &updateOptionFlagsEnabled,
            &timeBeforeReqFWData);

        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Parsing UpdateComponent response failed: "
                      << "rc=" << rc << ",cc=" << static_cast<int>(cc) << "\n";
            return;
        }

        ordered_json data;
        fillCompletionCode(cc, data, PLDM_FWUP);

        if (cc == PLDM_SUCCESS)
        {
            // Possible values:
            // 0 – Component can be updated,
            // 1 – Component will not be updated
            data["ComponentCompatibilityResponse"] =
                componentCompatibilityResp ? "Component will not be updated"
                                           : "Component can be updated";

            data["ComponentCompatibilityResponseCode"] =
                std::format("0x{:02X}", componentCompatibilityRespCode);
            data["UpdateOptionFlagsEnabled"] =
                std::to_string(updateOptionFlagsEnabled.value);
            data["EstimatedTimeBeforeSendingRequestFirmwareData"] =
                std::to_string(timeBeforeReqFWData) + "s";
        }

        pldmtool::helper::DisplayInJson(data);
    }

  private:
    uint16_t componentClassification;
    uint16_t componentIdentifier;
    uint8_t componentClassificationIndex;
    uint32_t componentComparisonStamp;
    uint32_t componentImageSize;
    std::string strUpdateOptionFlags;
    std::string componentVersionStringType;
    uint8_t componentVersionStringLength;
    std::string componentVersionString;
};

class ActivateFirmware : public CommandInterface
{
  public:
    ~ActivateFirmware() = default;
    ActivateFirmware() = delete;
    ActivateFirmware(const ActivateFirmware&) = delete;
    ActivateFirmware(ActivateFirmware&&) = delete;
    ActivateFirmware& operator=(const ActivateFirmware&) = delete;
    ActivateFirmware& operator=(ActivateFirmware&&) = delete;

    explicit ActivateFirmware(const char* type, const char* name,
                              CLI::App* app) : CommandInterface(type, name, app)
    {
        app->add_option("--self_contained_activation_request",
                        selfContainedActivRequest,
                        "Self contained activation request")
            ->required()
            ->transform(CLI::CheckedTransformer(pldmSelfContainedActivation,
                                                CLI::ignore_case));
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = new (requestMsg.data()) pldm_msg;
        auto rc = encode_activate_firmware_req(
            instanceId, selfContainedActivRequest, request,
            sizeof(pldm_activate_firmware_req));

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint16_t estimatedTimeForActivation = 0;

        auto rc = decode_activate_firmware_resp(responsePtr, payloadLength, &cc,
                                                &estimatedTimeForActivation);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Parsing ActivateFirmware response failed: "
                      << "rc=" << rc << ",cc=" << static_cast<int>(cc) << "\n";
            return;
        }

        ordered_json data;
        fillCompletionCode(cc, data, PLDM_FWUP);

        if (cc == PLDM_SUCCESS)
        {
            data["EstimatedTimeForSelfContainedActivation"] =
                std::to_string(estimatedTimeForActivation) + "s";
        }

        pldmtool::helper::DisplayInJson(data);
    }

  private:
    bool8_t selfContainedActivRequest;
};

class CancelUpdateComponent : public CommandInterface
{
  public:
    ~CancelUpdateComponent() = default;
    CancelUpdateComponent() = delete;
    CancelUpdateComponent(const CancelUpdateComponent&) = delete;
    CancelUpdateComponent(CancelUpdateComponent&&) = delete;
    CancelUpdateComponent& operator=(const CancelUpdateComponent&) = delete;
    CancelUpdateComponent& operator=(CancelUpdateComponent&&) = delete;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = new (requestMsg.data()) pldm_msg;
        auto rc = encode_cancel_update_component_req(
            instanceId, request, PLDM_CANCEL_UPDATE_COMPONENT_REQ_BYTES);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;

        auto rc = decode_cancel_update_component_resp(responsePtr,
                                                      payloadLength, &cc);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Parsing CancelUpdateComponent response failed: "
                      << "rc=" << rc << ",cc=" << static_cast<int>(cc) << "\n";
            return;
        }

        ordered_json data;
        fillCompletionCode(cc, data, PLDM_FWUP);

        pldmtool::helper::DisplayInJson(data);
    }
};

class CancelUpdate : public CommandInterface
{
  public:
    ~CancelUpdate() = default;
    CancelUpdate() = delete;
    CancelUpdate(const CancelUpdate&) = delete;
    CancelUpdate(CancelUpdate&&) = delete;
    CancelUpdate& operator=(const CancelUpdate&) = delete;
    CancelUpdate& operator=(CancelUpdate&&) = delete;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = new (requestMsg.data()) pldm_msg;
        auto rc = encode_cancel_update_req(instanceId, request,
                                           PLDM_CANCEL_UPDATE_REQ_BYTES);

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        bool8_t nonFunctioningComponentIndication;
        bitfield64_t nonFunctioningComponentBitmap{0};
        auto rc = decode_cancel_update_resp(responsePtr, payloadLength, &cc,
                                            &nonFunctioningComponentIndication,
                                            &nonFunctioningComponentBitmap);
        if (rc != PLDM_SUCCESS)
        {
            std::cerr << "Parsing CancelUpdate response failed: "
                      << "rc=" << rc << ",cc=" << static_cast<int>(cc) << "\n";
            return;
        }

        ordered_json data;

        fillCompletionCode(cc, data, PLDM_FWUP);

        if (cc == PLDM_SUCCESS)
        {
            data["NonFunctioningComponentIndication"] =
                nonFunctioningComponentIndication ? "True" : "False";

            if (nonFunctioningComponentIndication)
            {
                data["NonFunctioningComponentBitmap"] =
                    std::to_string(nonFunctioningComponentBitmap.value);
            }
        }

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

    auto queryDeviceIdentifiers = fwUpdate->add_subcommand(
        "QueryDeviceIdentifiers", "To query device identifiers of the FD");
    commands.push_back(std::make_unique<QueryDeviceIdentifiers>(
        "fw_update", "QueryDeviceIdentifiers", queryDeviceIdentifiers));

    auto requestUpdate = fwUpdate->add_subcommand(
        "RequestUpdate", "To initiate a firmware update");
    commands.push_back(std::make_unique<RequestUpdate>(
        "fw_update", "RequestUpdate", requestUpdate));

    auto passCompTable = fwUpdate->add_subcommand("PassComponentTable",
                                                  "To pass component table");
    commands.push_back(std::make_unique<PassComponentTable>(
        "fw_update", "PassComponentTable", passCompTable));

    auto updateComp = fwUpdate->add_subcommand(
        "UpdateComponent", "To request updating a specific firmware component");
    commands.push_back(std::make_unique<UpdateComponent>(
        "fw_update", "UpdateComponent", updateComp));

    auto activateFirmware =
        fwUpdate->add_subcommand("ActivateFirmware", "To activate firmware");
    commands.push_back(std::make_unique<ActivateFirmware>(
        "fw_update", "ActivateFirmware", activateFirmware));

    auto cancelUpdateComp = fwUpdate->add_subcommand(
        "CancelUpdateComponent", "To cancel component update");
    commands.push_back(std::make_unique<CancelUpdateComponent>(
        "fw_update", "CancelUpdateComponent", cancelUpdateComp));

    auto cancelUpdate =
        fwUpdate->add_subcommand("CancelUpdate", "To cancel update");
    commands.push_back(std::make_unique<CancelUpdate>(
        "fw_update", "CancelUpdate", cancelUpdate));
}

} // namespace fw_update

} // namespace pldmtool
