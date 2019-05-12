#include "libpldmresponder/platform.hpp"
#include "libpldmresponder/pdr.hpp"

#include <gtest/gtest.h>
#include <iostream>

using namespace pldm::responder;

TEST(getPDR, testGoodPath)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_GET_PDR_MIN_RESP_BYTES + 100> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload request{};
    std::array<uint8_t, PLDM_GET_PDR_REQ_BYTES> requestPayload{};

    request.payload = requestPayload.data();
    uint8_t* start = request.payload;
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* reqCount = reinterpret_cast<uint16_t*>(start);
    *reqCount = 100;
    request.payload_length = requestPayload.size();
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    getPDR(&request, &response);
    ASSERT_EQ(response.body.payload[0], PLDM_SUCCESS);
    start = response.body.payload;
    start += sizeof(uint8_t);
    uint32_t* nextRecordHandle = reinterpret_cast<uint32_t*>(start);
    ASSERT_EQ(*nextRecordHandle, 2);
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* recordCount = reinterpret_cast<uint16_t*>(start);
    ASSERT_EQ(*recordCount != 0, true);
    start += sizeof(uint16_t);
    // Check a bit of the PDR common header
    pldm_state_effecter_pdr* pdr = reinterpret_cast<pldm_state_effecter_pdr*>(start);
    ASSERT_EQ(pdr->hdr.record_handle, 1);
    ASSERT_EQ(pdr->hdr.version, 1);
}

TEST(getPDR, testShortRead)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_GET_PDR_MIN_RESP_BYTES + 1> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload request{};
    std::array<uint8_t, PLDM_GET_PDR_REQ_BYTES> requestPayload{};

    request.payload = requestPayload.data();
    uint8_t* start = request.payload;
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* reqCount = reinterpret_cast<uint16_t*>(start);
    // Read 1 byte of PDR
    *reqCount = 1;
    request.payload_length = requestPayload.size();
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    getPDR(&request, &response);
    ASSERT_EQ(response.body.payload[0], PLDM_SUCCESS);
    start = response.body.payload;
    start += sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* recordCount = reinterpret_cast<uint16_t*>(start);
    ASSERT_EQ(*recordCount, 1);
}

TEST(getPDR, testBadRecordHandle)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_GET_PDR_MIN_RESP_BYTES + 1> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload request{};
    std::array<uint8_t, PLDM_GET_PDR_REQ_BYTES> requestPayload{};

    request.payload = requestPayload.data();
    uint8_t* start = request.payload;
    uint32_t* recordHandle = reinterpret_cast<uint32_t*>(start);
    *recordHandle = 100000;
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* reqCount = reinterpret_cast<uint16_t*>(start);
    *reqCount = 1;
    request.payload_length = requestPayload.size();
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    getPDR(&request, &response);
    ASSERT_EQ(response.body.payload[0], PLDM_PLATFORM_INVALID_RECORD_HANDLE);
}

TEST(getPDR, testNoNextRecord)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_GET_PDR_MIN_RESP_BYTES + 1> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload request{};
    std::array<uint8_t, PLDM_GET_PDR_REQ_BYTES> requestPayload{};

    request.payload = requestPayload.data();
    uint8_t* start = request.payload;
    uint32_t* recordHandle = reinterpret_cast<uint32_t*>(start);
    *recordHandle = 3;
    request.payload_length = requestPayload.size();
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    getPDR(&request, &response);
    ASSERT_EQ(response.body.payload[0], PLDM_SUCCESS);
    start = response.body.payload;
    start += sizeof(uint8_t);
    uint32_t* nextRecordHandle = reinterpret_cast<uint32_t*>(start);
    ASSERT_EQ(*nextRecordHandle, 0);
}

TEST(getPDR, testFindPDR)
{
    pldm_msg response{};
    std::array<uint8_t, PLDM_GET_PDR_MIN_RESP_BYTES + 100> responseMsg{};
    response.body.payload = responseMsg.data();
    response.body.payload_length = responseMsg.size();
    pldm_msg_payload request{};
    std::array<uint8_t, PLDM_GET_PDR_REQ_BYTES> requestPayload{};

    request.payload = requestPayload.data();
    uint8_t* start = request.payload;
    start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    uint16_t* reqCount = reinterpret_cast<uint16_t*>(start);
    *reqCount = 100;
    request.payload_length = requestPayload.size();
    using namespace pdr;
    Repo& pdrRepo = get("./pdr_jsons/state_effecter/good");
    ASSERT_EQ(pdrRepo.empty(), false);
    getPDR(&request, &response);

    // Let's try to find a PDR of type stateEffecter (= 11) and entity type = 100
    bool found = false;
    uint32_t handle = 0; // start asking for PDRs from recordHandle 0
    uint32_t* recordHandle = nullptr;
    while (!found)
    {
        start = request.payload;
        recordHandle = reinterpret_cast<uint32_t*>(start);
        *recordHandle = handle;
        getPDR(&request, &response);
        ASSERT_EQ(response.body.payload[0], PLDM_SUCCESS);
        start = response.body.payload;
        start += sizeof(uint8_t);
        uint32_t* nextRecordHandle = reinterpret_cast<uint32_t*>(start);
        handle = *nextRecordHandle; // point to the next pdr in case current is not what we want
        start += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t);
        pldm_pdr_hdr* hdr = reinterpret_cast<pldm_pdr_hdr*>(start);
uint32_t intType = hdr->type;
std::cerr << "PDR next record handle " << handle << std::endl;
std::cerr << "PDR type " << intType << std::endl;
        if (hdr->type == PLDM_STATE_EFFECTER_PDR)
        {
            pldm_state_effecter_pdr* pdr = reinterpret_cast<pldm_state_effecter_pdr*>(start);
std::cerr << "PDR entity type " << pdr->entity_type << std::endl;
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
