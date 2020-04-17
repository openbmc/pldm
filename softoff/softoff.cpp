#include "config.h"

#include "softoff.hpp"

#include "utils.hpp"

#include <unistd.h>

#include <CLI/CLI.hpp>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

constexpr auto timeOutJsonPath = "/usr/share/pldm/softoff/softoff.json";
namespace pldm
{

using namespace phosphor::logging;
using Json = nlohmann::json;
namespace fs = std::filesystem;
using sdbusplus::exception::SdBusError;

using namespace sdbusplus::xyz::openbmc_project::State::server;
namespace sdbusRule = sdbusplus::bus::match::rules;

constexpr auto SYSTEMD_BUSNAME = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_PATH = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";
constexpr auto GUARD_CHASSIS_OFF_TARGET_SERVICE =
    "guardChassisOffTarget@.service";

constexpr auto PLDM_POWER_OFF_PATH =
    "/xyz/openbmc_project/pldm/softoff"; // This path is just an example. The
                                         // PLDM_POWER_OFF_PATH need to be
                                         // creat.
constexpr auto PLDM_POWER_OFF_INTERFACE =
    "xyz.openbmc_project.Pldm.Softoff"; // This interface is just an example.
                                        // The PLDM_POWER_OFF_INTERFACE need to
                                        // be creat.

PldmSoftPowerOff::PldmSoftPowerOff(sdbusplus::bus::bus& bus, sd_event* event) :
    bus(bus), timer(event),
    hostTransitionMatch(bus,
                        sdbusRule::propertiesChanged(PLDM_POWER_OFF_PATH,
                                                     PLDM_POWER_OFF_INTERFACE),
                        std::bind(&PldmSoftPowerOff::setHostSoftOffCompleteFlag,
                                  this, std::placeholders::_1))
{
    // Send soft off to host
    this->set_mctpEid_effecterId_state(
        HOST_SOFTOFF_MCTP_ID, HOST_SOFTOFF_EFFECTER_ID, HOST_SOFTOFF_STATE);
    auto rc = this->setStateEffecterStates();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message setStateEffecterStates to host failure. PLDM "
                     "error code = "
                  << std::hex << std::showbase << rc << "\n";

        // This marks the completion of pldm soft power off.
        completed = true;
    }

    // Load json file get Timeout seconds
    this->parserJsonFile();

    // Start Timer
    using namespace std::chrono;
    auto time = duration_cast<microseconds>(seconds(timeOutSeconds));

    auto r = this->startTimer(time);
    if (r < 0)
    {
        log<level::ERR>("Failure to start Host soft off wait timer",
                        entry("ERRNO=0x%X", -r));
    }
    else
    {
        log<level::INFO>(
            "Timer started waiting for host soft off",
            entry("TIMEOUT_IN_MSEC=%llu",
                  (duration_cast<milliseconds>(seconds(timeOutSeconds)))
                      .count()));
    }
}

void PldmSoftPowerOff::setHostSoftOffCompleteFlag(
    sdbusplus::message::message& msg)
{

    namespace server = sdbusplus::xyz::openbmc_project::State::server;

    using DbusProperty = std::string;

    using Value =
        std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
                     int64_t, uint64_t, double, std::string>;

    std::string interface;
    std::map<DbusProperty, Value> properties;

    msg.read(interface, properties);

    if (properties.find("RequestedHostTransition") == properties.end())
    {
        return;
    }

    auto& requestedState =
        std::get<std::string>(properties.at("RequestedHostTransition"));

    if (server::Host::convertTransitionFromString(requestedState) ==
        server::Host::Transition::Off)
    {
        // Disable the timer
        auto r = timer.stop();
        if (r < 0)
        {
            log<level::ERR>("PLDM soft off: Failure to STOP the timer",
                            entry("ERRNO=0x%X", -r));
        }

        // This marks the completion of pldm soft power off.
        completed = true;
    }
}

int PldmSoftPowerOff::set_mctpEid_effecterId_state(uint8_t mctpEid,
                                                   uint16_t effecterId,
                                                   uint8_t state)
{
    this->mctpEid = mctpEid;
    this->effecterId = effecterId;
    this->state = state;

    return 0;
}

int PldmSoftPowerOff::setStateEffecterStates()
{
    uint8_t effecterCount = 1;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + sizeof(effecterId) +
                            sizeof(effecterCount) +
                            sizeof(set_effecter_state_field)>
        requestMsg{};
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    set_effecter_state_field stateField{PLDM_REQUEST_SET, state};
    auto rc = encode_set_state_effecter_states_req(0, effecterId, effecterCount,
                                                   &stateField, request);
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message encode failure. PLDM error code = " << std::hex
                  << std::showbase << rc << "\n";
        return -1;
    }

    // Open connection to MCTP socket
    int fd = pldm_open();
    if (-1 == fd)
    {
        std::cerr << "Failed to init mctp"
                  << "\n";
        return -1;
    }

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};
    // Send PLDM request msg and wait for response
    rc = pldm_send_recv(mctpEid, fd, requestMsg.data(), requestMsg.size(),
                        &responseMsg, &responseMsgSize);
    if (0 > rc)
    {
        std::cerr << "Failed to send message/receive response. RC = " << rc
                  << ", errno = " << errno << "\n";
        return -1;
    }
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg);
    std::cout << "Done. PLDM RC = " << std::hex << std::showbase
              << static_cast<uint16_t>(response->payload[0]) << std::endl;
    free(responseMsg);

    return 0;
}

int PldmSoftPowerOff::startTimer(const std::chrono::microseconds& usec)
{
    return timer.start(usec);
}

int PldmSoftPowerOff::parserJsonFile()
{
    fs::path dir(timeOutJsonPath);
    if (!fs::exists(dir) || fs::is_empty(dir))
    {
        std::cerr << "PLDM soft off time out JSON does not exist, PATH="
                  << timeOutJsonPath << "\n";
    }

    std::ifstream jsonFilePath(timeOutJsonPath);
    if (!jsonFilePath.is_open())
    {
        std::cerr << "Error opening PLDM soft off time out JSON file, PATH="
                  << timeOutJsonPath << "\n";
        return {};
    }

    // std::ifstream jsonFile(filePath);
    auto data = Json::parse(jsonFilePath);
    if (data.empty())
    {
        std::cerr << "Parsing PLDM soft off time out JSON file failed, FILE="
                  << timeOutJsonPath << "\n";
    }

    timeOutSeconds = data.value("softoff_timeout_secs", 0);

    return 0;
}

} // namespace pldm
