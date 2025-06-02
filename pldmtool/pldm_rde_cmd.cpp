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

void registerCommand(CLI::App& app)
{
    auto rde = app.add_subcommand("rde", "rde type command");
    rde->require_subcommand(1);
    auto negotiateRedfishParameters = rde->add_subcommand(
        "NegotiateRedfishParameters", "Negotiate Redfish Parameters");
    commands.push_back(std::make_unique<NegotiateRedfishParameters>(
        "rde", "NegotiateRedfishParameters", negotiateRedfishParameters));
}

} // namespace rde

} // namespace pldmtool
