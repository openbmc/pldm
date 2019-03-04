#include "libpldmresponder/base.hpp"
#include "registration.hpp"

#include <functional>

#include "libpldm/base.h"

#include <gtest/gtest.h>

TEST(RegisterHandler, testRegistration)
{
    using Type = uint8_t;
    using Cmd = uint8_t;
    static const std::map<Type, Cmd> capabilities{
        {PLDM_BASE, {PLDM_GET_PLDM_TYPES}}};

    auto handler = [](const pldm_msg_payload* request, pldm_msg* response) {
        uint8_t* result = reinterpret_cast<uint8_t*>(response);
        result[0] = 0;
        result[1] = 1;
        result[2] = 2;
    };

    pldm_msg response{};
    using namespace pldm::responder;
    registerHandler(PLDM_BASE, PLDM_GET_PLDM_TYPES, std::move(handler));
    invokeHandler(PLDM_BASE, PLDM_GET_PLDM_TYPES, nullptr, &response);
    uint8_t* result = reinterpret_cast<uint8_t*>(&response);
    ASSERT_EQ(result[0], 0);
    ASSERT_EQ(result[1], 1);
    ASSERT_EQ(result[2], 2);
}
