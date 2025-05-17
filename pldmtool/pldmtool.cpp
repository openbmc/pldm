#include "pldm_base_cmd.hpp"
#include "pldm_bios_cmd.hpp"
#include "pldm_cmd_helper.hpp"
#include "pldm_fru_cmd.hpp"
#include "pldm_fw_update_cmd.hpp"
#include "pldm_platform_cmd.hpp"
#include "pldmtool/oem/ibm/pldm_oem_ibm.hpp"

#include <CLI/CLI.hpp>

namespace pldmtool
{

namespace raw
{

using namespace pldmtool::helper;

namespace
{
std::vector<std::unique_ptr<CommandInterface>> commands;
}

class RawOp : public CommandInterface
{
  public:
    ~RawOp() = default;
    RawOp() = delete;
    RawOp(const RawOp&) = delete;
    RawOp(RawOp&&) = default;
    RawOp& operator=(const RawOp&) = delete;
    RawOp& operator=(RawOp&&) = delete;

    explicit RawOp(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-d,--data", rawData, "raw data")
            ->required()
            ->expected(-3);
    }
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override

    {
        rawData[0] = (rawData[0] & 0xe0) | instanceId;
        return {PLDM_SUCCESS, rawData};
    }

    void parseResponseMsg(pldm_msg* /* responsePtr */,
                          size_t /* payloadLength */) override
    {}

  private:
    std::vector<uint8_t> rawData;
};

void registerCommand(CLI::App& app)
{
    auto raw =
        app.add_subcommand("raw", "send a raw request and print response");
    commands.push_back(std::make_unique<RawOp>("raw", "raw", raw));
}

} // namespace raw

namespace mctpRaw
{

using namespace pldmtool::helper;

namespace
{
std::vector<std::unique_ptr<CommandInterface>> commands;
}

class MctpRawOp : public CommandInterface
{
  public:
    ~MctpRawOp() = default;
    MctpRawOp() = delete;
    MctpRawOp(const MctpRawOp&) = delete;
    MctpRawOp(MctpRawOp&&) = default;
    MctpRawOp& operator=(const MctpRawOp&) = delete;
    MctpRawOp& operator=(MctpRawOp&&) = delete;

    explicit MctpRawOp(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-e,--network-id", mctpNetworkId, "MCTP NetworkId")
            ->required();
        app->add_option("-d,--data", rawData, "raw MCTP data")
            ->required()
            ->expected(-3);
        app->add_flag("-p,--prealloc-tag", mctpPreAllocTag,
                      "use pre-allocated MCTP tag for this request");
    }
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override

    {
        return {PLDM_SUCCESS, rawData};
    }

    void parseResponseMsg(pldm_msg* /* responsePtr */,
                          size_t /* payloadLength */) override
    {}

  private:
    std::vector<uint8_t> rawData;
};

void registerCommand(CLI::App& app)
{
    auto mctpRaw = app.add_subcommand(
        "mctpRaw", "send an MCTP raw request and print response");
    commands.push_back(
        std::make_unique<MctpRawOp>("mctpRaw", "mctpRaw", mctpRaw));
}

} // namespace mctpRaw
} // namespace pldmtool

int main(int argc, char** argv)
{
    CLI::App app{"PLDM requester tool for OpenBMC"};
    app.require_subcommand(1)->ignore_case();

    pldmtool::raw::registerCommand(app);
    pldmtool::mctpRaw::registerCommand(app);
    pldmtool::base::registerCommand(app);
    pldmtool::bios::registerCommand(app);
    pldmtool::platform::registerCommand(app);
    pldmtool::fru::registerCommand(app);
    pldmtool::fw_update::registerCommand(app);

#ifdef OEM_IBM
    pldmtool::oem_ibm::registerCommand(app);
#endif

    CLI11_PARSE(app, argc, argv);
    pldmtool::platform::parseGetPDROption();
    return 0;
}
