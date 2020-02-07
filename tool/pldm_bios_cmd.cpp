#include "pldm_bios_cmd.hpp"

#include "pldm_cmd_helper.hpp"

#include "libpldm/utils.h"

namespace pldmtool
{

namespace bios
{

namespace
{

using namespace pldmtool::helper;

std::vector<std::unique_ptr<CommandInterface>> commands;

} // namespace

class GetDateTime : public CommandInterface
{
  public:
    ~GetDateTime() = default;
    GetDateTime() = delete;
    GetDateTime(const GetDateTime&) = delete;
    GetDateTime(GetDateTime&&) = default;
    GetDateTime& operator=(const GetDateTime&) = delete;
    GetDateTime& operator=(GetDateTime&&) = default;

    using CommandInterface::CommandInterface;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

        auto rc = encode_get_date_time_req(PLDM_LOCAL_INSTANCE_ID, request);
        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t cc = 0;

        uint8_t seconds, minutes, hours, day, month;
        uint16_t year;
        auto rc =
            decode_get_date_time_resp(responsePtr, payloadLength, &cc, &seconds,
                                      &minutes, &hours, &day, &month, &year);
        if (rc != PLDM_SUCCESS || cc != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)cc << std::endl;
            return;
        }
        std::cout << "Date & Time : " << std::endl;
        std::cout << "DD-MM-YYYY HH:MM:SS - ";
        setWidth(day);
        std::cout << "-";
        setWidth(month);
        std::cout << "-" << bcd2dec16(year) << " ";
        setWidth(hours);
        std::cout << ":";
        setWidth(minutes);
        std::cout << ":";
        setWidth(seconds);
        std::cout << std::endl;
    }

  private:
    void setWidth(uint8_t data)
    {
        std::cout << std::setfill('0') << std::setw(2)
                  << static_cast<uint32_t>(bcd2dec8(data));
    }
};

class SetDateTime : public CommandInterface
{
  public:
    ~SetDateTime() = default;
    SetDateTime() = delete;
    SetDateTime(const SetDateTime&) = delete;
    SetDateTime(SetDateTime&&) = default;
    SetDateTime& operator=(const SetDateTime&) = delete;
    SetDateTime& operator=(SetDateTime&&) = default;

    explicit SetDateTime(const char* type, const char* name, CLI::App* app) :
        CommandInterface(type, name, app)
    {
        app->add_option("-d,--data", tmData,
                        "set date time data\n"
                        "eg: YYYYMMDDHHMMSS")
            ->required();
    }

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        std::vector<uint8_t> requestMsg(sizeof(pldm_msg_hdr) +
                                        sizeof(struct pldm_set_date_time_req));
        auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
        uint16_t year = 0;
        uint8_t month = 0;
        uint8_t day = 0;
        uint8_t hours = 0;
        uint8_t minutes = 0;
        uint8_t seconds = 0;

        if (!uintToDate(tmData, &year, &month, &day, &hours, &minutes,
                        &seconds))
        {
            std::cerr << "decode date Error: "
                      << "tmData=" << tmData << std::endl;

            return {PLDM_ERROR_INVALID_DATA, requestMsg};
        }

        auto rc = encode_set_date_time_req(
            PLDM_LOCAL_INSTANCE_ID, seconds, minutes, hours, day, month, year,
            request, sizeof(struct pldm_set_date_time_req));

        return {rc, requestMsg};
    }

    void parseResponseMsg(pldm_msg* responsePtr, size_t payloadLength) override
    {
        uint8_t completionCode = 0;
        auto rc = decode_set_date_time_resp(responsePtr, payloadLength,
                                            &completionCode);

        if (rc != PLDM_SUCCESS || completionCode != PLDM_SUCCESS)
        {
            std::cerr << "Response Message Error: "
                      << "rc=" << rc << ",cc=" << (int)completionCode
                      << std::endl;
            return;
        }

        std::cout << "SetDateTime: SUCCESS" << std::endl;
    }

  private:
    uint64_t tmData;
};

void registerCommand(CLI::App& app)
{
    auto bios = app.add_subcommand("bios", "bios type command");
    bios->require_subcommand(1);
    auto getDateTime = bios->add_subcommand("GetDateTime", "get date time");
    commands.push_back(
        std::make_unique<GetDateTime>("bios", "GetDateTime", getDateTime));

    auto setDateTime =
        bios->add_subcommand("SetDateTime", "set host date time");
    commands.push_back(
        std::make_unique<SetDateTime>("bios", "setDateTime", setDateTime));
}

} // namespace bios

} // namespace pldmtool
