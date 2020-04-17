#include "config.h"

#include "softoff.hpp"

#include <array>
#include <iostream>
#include <nlohmann/json.hpp>

#include "libpldm/platform.h"
#include "libpldm/requester/pldm.h"

namespace pldm
{

constexpr auto timeOutJsonPath = "/usr/share/pldm/softoff/softoff.json";
constexpr auto HOST_SOFTOFF_MCTP_ID = 0;
constexpr auto HOST_SOFTOFF_EFFECTER_ID = 0;
constexpr auto HOST_SOFTOFF_STATE = 0;

using Json = nlohmann::json;
namespace fs = std::filesystem;

PldmSoftPowerOff::PldmSoftPowerOff()
{
    auto rc = setStateEffecterStates();
    if (rc != PLDM_SUCCESS)
    {
        std::cerr << "Message setStateEffecterStates to host failure. PLDM "
                     "error code = "
                  << std::hex << std::showbase << rc << "\n";
    }

    // Load json file get Timeout seconds
    parserJsonFile();
}

int PldmSoftPowerOff::setStateEffecterStates()
{
    uint8_t effecterCount = 1;
    std::array<uint8_t,
               sizeof(pldm_msg_hdr) + sizeof(HOST_SOFTOFF_EFFECTER_ID) +
                   sizeof(effecterCount) + sizeof(set_effecter_state_field)>
        requestMsg{};
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());
    set_effecter_state_field stateField{PLDM_REQUEST_SET, HOST_SOFTOFF_STATE};
    auto rc = encode_set_state_effecter_states_req(
        0, HOST_SOFTOFF_EFFECTER_ID, effecterCount, &stateField, request);
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
    rc = pldm_send_recv(HOST_SOFTOFF_MCTP_ID, fd, requestMsg.data(),
                        requestMsg.size(), &responseMsg, &responseMsgSize);
    if (rc < 0)
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

void PldmSoftPowerOff::parserJsonFile()
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
    }

    auto data = Json::parse(jsonFilePath);
    if (data.empty())
    {
        std::cerr << "Parsing PLDM soft off time out JSON file failed, FILE="
                  << timeOutJsonPath << "\n";
    }
    else
    {
        timeOutSeconds = data.value("softoff_timeout_secs", 0);
    }
}
} // namespace pldm
