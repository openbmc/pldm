#include "pldm_rde_cmd.hpp"

#include "common/utils.hpp"
#include "pldm_cmd_helper.hpp"

#include <libpldm/rde.h>
#include <libpldm/utils.h>

#include <map>
#include <optional>

namespace pldmtool
{

namespace rde
{

namespace
{

using namespace pldmtool::helper;
using namespace pldm::utils;

std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

class NegotiateRedfishParameters : public CommandInterface
{
  public:
    ~NegotiateRedfishParameters() = default;
    NegotiateRedfishParameters() = delete;
    NegotiateRedfishParameters(const NegotiateRedfishParameters&) = delete;
    NegotiateRedfishParameters(NegotiateRedfishParameters&&) = default;
    NegotiateRedfishParameters& operator=(const NegotiateRedfishParameters&) =
        delete;
    NegotiateRedfishParameters& operator=(NegotiateRedfishParameters&&) =
        delete;

    using CommandInterface::CommandInterface;

    explicit NegotiateRedfishParameters(const char* type, const char* name,
                                        CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option(
               "-c, --concurrency", concurrencySupport,
               "The maximum number of concurrent outstanding Operations"
               "the MC can support for this RDE Device"
               "Must be > 0; a value of 1 indicates no support for concurrency."
               "A value of 255 (0xFF) shall be interpreted to indicate"
               "that no such limit exists")
            ->required();
        app->add_option("-f, --feature", featureSupport.value,
                        "Operations and functionality supported by the MC;"
                        "for each, 1b indicates supported, 0b not:")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) +
            PLDM_RDE_NEGOTIATE_REDFISH_PARAMETERS_REQ_BYTES);
        auto request = new (requestMsg.data()) pldm_msg;

        auto rc = encode_negotiate_redfish_parameters_req(
            instanceId, concurrencySupport, &featureSupport, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;
        uint8_t deviceConcurrencySupport;
        bitfield8_t deviceCapabilitiesFlags;
        bitfield16_t deviceFeatureSupport;
        uint32_t deviceConfigurationSignature;
        struct pldm_rde_varstring providerName;

        auto rc = decode_negotiate_redfish_parameters_resp(
            responsePtr, payloadLength, &cc, &deviceConcurrencySupport,
            &deviceCapabilitiesFlags, &deviceFeatureSupport,
            &deviceConfigurationSignature, &providerName);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }

        std::stringstream dt;
        ordered_json data;
        dt << providerName.string_data;

        data["DeviceConcurrencySupport"] = deviceConcurrencySupport;
        data["DeviceCapabilitiesFlags"] = deviceCapabilitiesFlags.byte;
        data["DeviceFeatureSupport"] = deviceFeatureSupport.value;
        data["DeviceConfigurationSignature"] = deviceConfigurationSignature;
        data["DeviceProviderName.format"] = providerName.string_format;
        data["DeviceProviderName.length"] = providerName.string_length_bytes;
        data["DeviceProviderName"] = dt.str();

        pldmtool::helper::DisplayInJson(data);
    }

  private:
    uint8_t concurrencySupport;
    bitfield16_t featureSupport;
};

class NegotiateMediumParameters : public CommandInterface
{
  public:
    ~NegotiateMediumParameters() = default;
    NegotiateMediumParameters() = delete;
    NegotiateMediumParameters(const NegotiateMediumParameters&) = delete;
    NegotiateMediumParameters(NegotiateMediumParameters&&) = default;
    NegotiateMediumParameters& operator=(const NegotiateMediumParameters&) =
        delete;
    NegotiateMediumParameters& operator=(NegotiateMediumParameters&&) = delete;

    using CommandInterface::CommandInterface;

    explicit NegotiateMediumParameters(const char* type, const char* name,
                                       CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option(
               "-t, --transfersize", mcMaximumTransferSize,
               "An indication of the maximum amount of data"
               "the MC can support for a single message transfer."
               "This value represents the size of the PLDM header and PLDM payload;"
               "medium specific header information shall not be included in this calculation."
               "All MC implementations shall support a transfer size of at least 64 bytes.")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) +
            PLDM_RDE_NEGOTIATE_MEDIUM_PARAMETERS_REQ_BYTES);
        auto request = new (requestMsg.data()) pldm_msg;

        auto rc = encode_negotiate_medium_parameters_req(
            instanceId, mcMaximumTransferSize, request);

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t decodedCompletionCode;
        uint32_t decodedDeviceMaximumTransferSize;

        auto rc = decode_negotiate_medium_parameters_resp(
            responsePtr, payloadLength, &decodedCompletionCode,
            &decodedDeviceMaximumTransferSize);

        if (rc != PLDM_SUCCESS || decodedCompletionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)decodedCompletionCode
                      << std::endl;
            return;
        }

        ordered_json data;
        data["DeviceMaximumTransferChunkSizeBytes"] =
            decodedDeviceMaximumTransferSize;

        pldmtool::helper::DisplayInJson(data);
    }

  private:
    uint32_t mcMaximumTransferSize;
};

class GetSchemaDictionary : public CommandInterface
{
  public:
    ~GetSchemaDictionary() = default;
    GetSchemaDictionary() = delete;
    GetSchemaDictionary(const GetSchemaDictionary&) = delete;
    GetSchemaDictionary(GetSchemaDictionary&&) = default;
    GetSchemaDictionary& operator=(const GetSchemaDictionary&) = delete;
    GetSchemaDictionary& operator=(GetSchemaDictionary&&) = delete;

    using CommandInterface::CommandInterface;

    explicit GetSchemaDictionary(const char* type, const char* name,
                                 CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option(
               "-r, --resourceid", resourceID,
               "The ResourceID of any resource in the Redfish Resource PDR"
               "from which to retrieve the dictionary")
            ->required();
        app->add_option("-s, --schemaclass", schemaClass,
                        "The class of schema being requested")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(
            sizeof(pldm_msg_hdr) + PLDM_RDE_SCHEMA_DICTIONARY_REQ_BYTES);
        auto request = new (requestMsg.data()) pldm_msg;

        auto rc = encode_get_schema_dictionary_req(
            instanceId, resourceID, schemaClass,
            PLDM_RDE_SCHEMA_DICTIONARY_REQ_BYTES, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t decodeCompletionCode;
        uint8_t decodeDictionaryFormat;
        uint32_t decodeTransferHandle;

        auto rc = decode_get_schema_dictionary_resp(
            responsePtr, payloadLength, &decodeCompletionCode,
            &decodeDictionaryFormat, &decodeTransferHandle);
        if (rc != PLDM_SUCCESS || decodeCompletionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)decodeCompletionCode
                      << std::endl;
            return;
        }

        ordered_json data;
        data["DictionaryFormat"] = decodeDictionaryFormat;
        data["TransferHandle"] = decodeTransferHandle;

        pldmtool::helper::DisplayInJson(data);
    }

  private:
    uint32_t resourceID;
    uint8_t schemaClass;
};

void registerCommand(CLI::App& app)
{
    auto rde = app.add_subcommand("rde", "rde type command");
    rde->require_subcommand(1);
    auto negotiateRedfishParameters = rde->add_subcommand(
        "NegotiateRedfishParameters", "Negotiate Redfish Parameters");
    commands.push_back(std::make_unique<NegotiateRedfishParameters>(
        "rde", "NegotiateRedfishParameters", negotiateRedfishParameters));

    auto negotiateMediumParameters = rde->add_subcommand(
        "NegotiateMediumParameters", "Negotiate Medium Parameters");
    commands.push_back(std::make_unique<NegotiateMediumParameters>(
        "rde", "NegotiateMediumParameters", negotiateMediumParameters));

    auto getSchemaDictionary =
        rde->add_subcommand("GetSchemaDictionary", "Get Schema Dictionary");
    commands.push_back(std::make_unique<GetSchemaDictionary>(
        "rde", "GetSchemaDictionary", getSchemaDictionary));
}

} // namespace rde

} // namespace pldmtool
