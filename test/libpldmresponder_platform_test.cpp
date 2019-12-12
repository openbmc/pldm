#include "libpldmresponder/effecters.hpp"
#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/platform.hpp"

#include <iostream>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace pldm::responder;
using namespace pldm::responder::pdr;

TEST(getPDR, testGoodPath)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t* start = request->payload;
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* reqCount = reinterpret_cast<uint16_t*>(start);
    *reqCount = 100;
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    platform::Handler handler;
    auto response = handler.getPDR(request, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    start = responsePtr->payload;
    start += sizeof(uint8_t);
    uint32_t* nextRecordHandle = reinterpret_cast<uint32_t*>(start);
    ASSERT_EQ(*nextRecordHandle, 2);
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* recordCount = reinterpret_cast<uint16_t*>(start);
    ASSERT_EQ(*recordCount != 0, true);
    start += sizeof(uint16_t);
    // Check a bit of the PDR common header
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(start);
    ASSERT_EQ(pdr->hdr.record_handle, 1);
    ASSERT_EQ(pdr->hdr.version, 1);
}

TEST(getPDR, testShortRead)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t* start = request->payload;
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* reqCount = reinterpret_cast<uint16_t*>(start);
    // Read 1 byte of PDR
    *reqCount = 1;
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    platform::Handler handler;
    auto response = handler.getPDR(request, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    start = responsePtr->payload;
    start +=
        sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* recordCount = reinterpret_cast<uint16_t*>(start);
    ASSERT_EQ(*recordCount, 1);
}

TEST(getPDR, testBadRecordHandle)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t* start = request->payload;
    uint32_t* recordHandle = reinterpret_cast<uint32_t*>(start);
    *recordHandle = 100000;
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* reqCount = reinterpret_cast<uint16_t*>(start);
    *reqCount = 1;
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    platform::Handler handler;
    auto response = handler.getPDR(request, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    ASSERT_EQ(responsePtr->payload[0], PLDM_PLATFORM_INVALID_RECORD_HANDLE);
}

TEST(getPDR, testNoNextRecord)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t* start = request->payload;
    uint32_t* recordHandle = reinterpret_cast<uint32_t*>(start);
    *recordHandle = 3;
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    platform::Handler handler;
    auto response = handler.getPDR(request, requestPayloadLength);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    start = responsePtr->payload;
    start += sizeof(uint8_t);
    uint32_t* nextRecordHandle = reinterpret_cast<uint32_t*>(start);
    ASSERT_EQ(*nextRecordHandle, 0);
}

TEST(getPDR, testFindPDR)
{
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_GET_PDR_REQ_BYTES>
        requestPayload{};
    auto request = reinterpret_cast<pldm_msg*>(requestPayload.data());
    size_t requestPayloadLength = requestPayload.size() - sizeof(pldm_msg_hdr);

    uint8_t* start = request->payload;
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* reqCount = reinterpret_cast<uint16_t*>(start);
    *reqCount = 100;
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    platform::Handler handler;
    auto response = handler.getPDR(request, requestPayloadLength);

    // Let's try to find a PDR of type stateEffecter (= 11) and entity type =
    // 100
    bool found = false;
    uint32_t handle = 0; // start asking for PDRs from recordHandle 0
    uint32_t* recordHandle = nullptr;
    while (!found)
    {
        start = request->payload;
        recordHandle = reinterpret_cast<uint32_t*>(start);
        *recordHandle = handle;
        platform::Handler handler;
        auto response = handler.getPDR(request, requestPayloadLength);
        auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

        ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
        start = responsePtr->payload;
        start += sizeof(uint8_t);
        uint32_t* nextRecordHandle = reinterpret_cast<uint32_t*>(start);
        handle = *nextRecordHandle; // point to the next pdr in case current is
                                    // not what we want
        start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) +
                 sizeof(uint16_t);
        pldm_pdr_hdr* hdr = reinterpret_cast<pldm_pdr_hdr*>(start);
        uint32_t intType = hdr->type;
        std::cerr << "PDR next record handle " << handle << "\n";
        std::cerr << "PDR type " << intType << "\n";
        if (hdr->type == PLDM_STATE_EFFECTER_PDR)
        {
            pldm_state_effecter_pdr* pdr =
                reinterpret_cast<pldm_state_effecter_pdr*>(start);
            std::cerr << "PDR entity type " << pdr->entity_type << "\n";
            if (pdr->entity_type == 100)
            {
                found = true;
                // Rest of the PDR can be accessed as need be
                break;
            }
        }
        if (!*nextRecordHandle) // no more records
        {
            break;
        }
    }
    ASSERT_EQ(found, true);
}

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
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
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
        handlerObj, 0x1, stateField);
    ASSERT_EQ(rc, 0);
}

TEST(setStateEffecterStatesHandler, testBadRequest)
{
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
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
        handlerObj, 0x1, stateField);
    ASSERT_EQ(rc, PLDM_PLATFORM_SET_EFFECTER_UNSUPPORTED_SENSORSTATE);

    rc = handler.setStateEffecterStatesHandler<MockdBusHandler>(handlerObj, 0x9,
                                                                stateField);
    ASSERT_EQ(rc, PLDM_PLATFORM_INVALID_EFFECTER_ID);

    stateField.push_back({PLDM_REQUEST_SET, 4});
    rc = handler.setStateEffecterStatesHandler<MockdBusHandler>(handlerObj, 0x1,
                                                                stateField);
    ASSERT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    std::vector<set_effecter_state_field> newStateField;
    newStateField.push_back({PLDM_REQUEST_SET, 1});

    rc = handler.setStateEffecterStatesHandler<MockdBusHandler>(handlerObj, 0x2,
                                                                newStateField);
    ASSERT_EQ(rc, PLDM_PLATFORM_INVALID_STATE_VALUE);
}
