#include "libpldmresponder/platform.hpp"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::responder;
using namespace pldm::responder::pdr::internal;

namespace pldm
{

namespace responder
{

class MockdBusHandler
{
  public:
    MOCK_CONST_METHOD4(setDbusProperty,
                       int(const std::string&, const std::string&,
                           const std::string&,
                           const std::variant<std::string>&));
};
} // namespace responder
} // namespace pldm

using ::testing::_;
using ::testing::Return;

TEST(setStateEffecterStatesHandler, testGoodRequest)
{
    IndexedRepo pdrRepo;
    getRepoByType("./pdr_jsons/state_effecter/good", PLDM_STATE_EFFECTER_PDR,
                  pdrRepo);
    pdr::Entry e = pdrRepo.at(1);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(e.data());
    EXPECT_EQ(pdr->hdr.type, PLDM_STATE_EFFECTER_PDR);

    std::vector<set_effecter_state_field> stateField;
    stateField.push_back({PLDM_REQUEST_SET, 1});
    stateField.push_back({PLDM_REQUEST_SET, 1});

    auto bootProgressInf = "xyz.openbmc_project.State.OperatingSystem.Status";
    auto bootProgressProp = "OperatingSystemState";
    std::string objPath = "/foo/bar";
    std::variant<std::string> value{"xyz.openbmc_project.State.OperatingSystem."
                                    "Status.OSStatus.Standby"};

    MockdBusHandler handlerObj;
    EXPECT_CALL(handlerObj, setDbusProperty(objPath, bootProgressProp,
                                            bootProgressInf, value))
        .Times(2);
    platform::Handler handler;
    auto rc = handler.setStateEffecterStatesHandler<MockdBusHandler>(
        handlerObj, pdrRepo, 0x1, stateField);
    ASSERT_EQ(rc, 0);
}

TEST(setStateEffecterStatesHandler, testBadRequest)
{
    IndexedRepo pdrRepo;
    getRepoByType("./pdr_jsons/state_effecter/good", PLDM_STATE_EFFECTER_PDR,
                  pdrRepo);
    pdr::Entry e = pdrRepo.at(1);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(e.data());
    EXPECT_EQ(pdr->hdr.type, PLDM_STATE_EFFECTER_PDR);

    std::vector<set_effecter_state_field> stateField;
    stateField.push_back({PLDM_REQUEST_SET, 3});
    stateField.push_back({PLDM_REQUEST_SET, 4});

    MockdBusHandler handlerObj;
    platform::Handler handler;
    auto rc = handler.setStateEffecterStatesHandler<MockdBusHandler>(
        handlerObj, pdrRepo, 0x1, stateField);
    ASSERT_EQ(rc, PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE);

    rc = handler.setStateEffecterStatesHandler<MockdBusHandler>(
        handlerObj, pdrRepo, 0x9, stateField);
    ASSERT_EQ(rc, PLDM_PLATFORM_INVALID_EFFECTER_ID);

    stateField.push_back({PLDM_REQUEST_SET, 4});
    rc = handler.setStateEffecterStatesHandler<MockdBusHandler>(
        handlerObj, pdrRepo, 0x1, stateField);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    std::vector<set_effecter_state_field> newStateField;
    newStateField.push_back({PLDM_REQUEST_SET, 1});

    rc = handler.setStateEffecterStatesHandler<MockdBusHandler>(
        handlerObj, pdrRepo, 0x2, newStateField);
    ASSERT_EQ(rc, PLDM_PLATFORM_INVALID_STATE_VALUE);
}
