#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/platform.hpp"

#include <iostream>

#include <gtest/gtest.h>

using namespace pldm::responder;

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
    auto response = getPDR(request, requestPayloadLength);
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
    auto response = getPDR(request, requestPayloadLength);
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
    auto response = getPDR(request, requestPayloadLength);
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
    auto response = getPDR(request, requestPayloadLength);
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
    auto response = getPDR(request, requestPayloadLength);

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
        auto response = getPDR(request, requestPayloadLength);
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
        std::cerr << "PDR next record handle " << handle << std::endl;
        std::cerr << "PDR type " << intType << std::endl;
        if (hdr->type == PLDM_STATE_EFFECTER_PDR)
        {
            pldm_state_effecter_pdr* pdr =
                reinterpret_cast<pldm_state_effecter_pdr*>(start);
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
