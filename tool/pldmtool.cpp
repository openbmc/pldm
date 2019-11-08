#include "pldm_base_cmd.hpp"
#include "pldm_cmd_helper.hpp"

#include <CLI/CLI.hpp>

class RawOp : public CommandInterface
{
  public:
    ~RawOp() = default;
    RawOp() = delete;
    RawOp(const RawOp&) = delete;
    RawOp(RawOp&&) = default;
    RawOp& operator=(const RawOp&) = delete;
    RawOp& operator=(RawOp&&) = default;

    explicit RawOp(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-d,--data", rawData, "raw data")
            ->required()
            ->expected(-3);
    }
    std::pair<int, std::vector<uint8_t>> createRequestMsg() override

    {
        return {PLDM_SUCCESS, rawData};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        std::ignore = responsePtr;
        std::ignore = payloadLength;
    }

  private:
    std::vector<uint8_t> rawData;
};

int main(int argc, char** argv)
{

    CLI::App app{"PLDM requester tool for OpenBMC"};
    app.require_subcommand(1)->ignore_case();
    auto raw = app.add_subcommand("raw", "raw command");
    RawOp rawOP("raw", "raw", raw);

    registerBaseCommand(app);

    CLI11_PARSE(app, argc, argv);
}
