#include "libpldmresponder/platform.hpp"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::responder;
using namespace pldm::responder::pdr::internal;
using namespace pldm::responder::platformNumericEffecter;

namespace pldm
{

namespace responder
{

class MockdBusHandler
{
  public:
    MOCK_CONST_METHOD4(setDbusProperty,
                       int(const std::string&, const std::string&,
                           const std::string&, const std::variant<uint64_t>&));
};
} // namespace responder
} // namespace pldm

using ::testing::_;
using ::testing::Return;

TEST(setNumericEffecterValueHandler, testGoodRequest)
{
    IndexedRepo pdrRepo;
    getRepoByType("./pdr_jsons/state_effecter/good", PLDM_NUMERIC_EFFECTER_PDR,
                  pdrRepo);
    pdr::Entry e = pdrRepo.at(1);
    pldm_numeric_effecter_value_pdr* pdr =
        reinterpret_cast<pldm_numeric_effecter_value_pdr*>(e.data());
    EXPECT_EQ(pdr->hdr.type, PLDM_NUMERIC_EFFECTER_PDR);

    uint16_t effecterId = PLDM_TIMED_POWER_ON_STATE;
    uint64_t effecterValue = 9876543210;

    auto bootProgressInf = "xyz.openbmc_project.foo.bar";
    auto bootProgressProp = "property";
    std::string objPath = "/xyz/openbmc_project/foo/bar";
    std::variant<uint64_t> value{effecterValue};

    MockdBusHandler handlerObj;
    EXPECT_CALL(handlerObj, setDbusProperty(objPath, bootProgressProp,
                                            bootProgressInf, value))
        .Times(1);
    auto rc = setNumericEffecterValueHandler<MockdBusHandler>(
        handlerObj, pdrRepo, effecterId, PLDM_EFFECTER_DATA_SIZE_UINT32,
        reinterpret_cast<uint8_t*>(&effecterValue), 4);
    ASSERT_EQ(rc, 0);
}

TEST(setNumericEffecterValueHandler, testBadRequest)
{
    IndexedRepo pdrRepo;
    getRepoByType("./pdr_jsons/state_effecter/good", PLDM_NUMERIC_EFFECTER_PDR,
                  pdrRepo);
    pdr::Entry e = pdrRepo.at(1);
    pldm_numeric_effecter_value_pdr* pdr =
        reinterpret_cast<pldm_numeric_effecter_value_pdr*>(e.data());
    EXPECT_EQ(pdr->hdr.type, PLDM_NUMERIC_EFFECTER_PDR);

    uint16_t effecterId = PLDM_TIMED_POWER_ON_STATE;
    uint64_t effecterValue = 9876543210;
    MockdBusHandler handlerObj;
    auto rc = setNumericEffecterValueHandler<MockdBusHandler>(
        handlerObj, pdrRepo, effecterId, PLDM_EFFECTER_DATA_SIZE_SINT32,
        reinterpret_cast<uint8_t*>(&effecterValue), 4);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    pdrRepo.makeEmpty();
    rc = setNumericEffecterValueHandler<MockdBusHandler>(
        handlerObj, pdrRepo, effecterId, PLDM_EFFECTER_DATA_SIZE_UINT32,
        reinterpret_cast<uint8_t*>(&effecterValue), 4);
    ASSERT_EQ(rc, PLDM_ERROR);
}