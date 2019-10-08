#include <CLI/CLI.hpp>
#include <array>
#include <iostream>

#include "libpldm/platform.h"
#include "libpldm/transport/mctp.h"

int main(int argc, char** argv)
{
    CLI::App app{"Send PLDM command SetStateEffecterStates"};

    uint8_t mctpEid{};
    app.add_option("-m,--mctp_eid", mctpEid, "MCTP EID")->required();
    uint16_t effecterId{};
    app.add_option("-e,--effecter", effecterId, "Effecter Id")->required();
    uint8_t state{};
    app.add_option("-s,--state", state, "New state value")->required();

    CLI11_PARSE(app, argc, argv);

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
                  << std::showbase << rc << std::endl;
        return -1;
    }

    uint8_t* responseMsg = nullptr;
    size_t responseMsgSize{};
    int fd = pldm_mctp_init();
    if (-1 == fd)
    {
        std::cerr << "Failed to init mctp" << std::endl;
        return -1;
    }
    rc = pldm_send_recv(mctpEid, fd, requestMsg.data(), requestMsg.size(),
                        &responseMsg, &responseMsgSize);
    if (0 > rc)
    {
        std::cerr << "Failed to send message/receive response. RC = " << rc
                  << ", errno = " << errno << std::endl;
        return -1;
    }
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg + 2);
    std::cout << "Done. PLDM RC = " << std::hex << std::showbase
              << static_cast<uint16_t>(response->payload[0]) << std::endl;

    return 0;
}
