#include "../oem_event_manager.hpp"

#include <libpldm/base.h>

#include <vector>

#include <gtest/gtest.h>

using namespace pldm::oem_meta;

namespace
{

std::vector<uint8_t> makeRequestBuffer(size_t payloadSize)
{
    return std::vector<uint8_t>(sizeof(pldm_msg_hdr) + payloadSize, 0);
}

} // namespace

TEST(OemEventManagerTest, EmptyPayloadRejected)
{
    OemEventManager manager;
    auto buffer = makeRequestBuffer(0);
    auto* request = reinterpret_cast<pldm_msg*>(buffer.data());

    EXPECT_EQ(manager.handleOemEvent(request, 0, 0, 0, 0),
              PLDM_ERROR_INVALID_LENGTH);
}

TEST(OemEventManagerTest, OneBytePayloadRejected)
{
    OemEventManager manager;
    auto buffer = makeRequestBuffer(1);
    auto* request = reinterpret_cast<pldm_msg*>(buffer.data());

    EXPECT_EQ(manager.handleOemEvent(request, 1, 0, 0, 0),
              PLDM_ERROR_INVALID_LENGTH);
}

TEST(OemEventManagerTest, ThirteenBytePayloadRejected)
{
    OemEventManager manager;
    auto buffer = makeRequestBuffer(13);
    auto* request = reinterpret_cast<pldm_msg*>(buffer.data());

    EXPECT_EQ(manager.handleOemEvent(request, 13, 0, 0, 0),
              PLDM_ERROR_INVALID_LENGTH);
}

TEST(OemEventManagerTest, FourteenBytePayloadPassesLengthCheck)
{
    OemEventManager manager;
    auto buffer = makeRequestBuffer(14);
    auto* request = reinterpret_cast<pldm_msg*>(buffer.data());
    // Unsupported record type so the call returns deterministically without
    // reaching the D-Bus commit path; this test only exercises the length
    // check.
    request->payload[0] = 0xFF;

    EXPECT_EQ(manager.handleOemEvent(request, 14, 0, 0, 0), PLDM_ERROR);
}

TEST(OemEventManagerTest, OffsetGreaterThanPayloadLengthRejected)
{
    OemEventManager manager;
    auto buffer = makeRequestBuffer(14);
    auto* request = reinterpret_cast<pldm_msg*>(buffer.data());

    EXPECT_EQ(manager.handleOemEvent(request, 14, 0, 0, 15),
              PLDM_ERROR_INVALID_LENGTH);
}
