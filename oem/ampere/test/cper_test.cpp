#include "libcper/Cper.h"

#include "cper.hpp"

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

using namespace pldm::oem_ampere;

namespace
{

constexpr size_t kCommon = sizeof(CommonEventData);
const size_t kHdr = sizeof(EFI_COMMON_ERROR_RECORD_HEADER);
const size_t kDesc = sizeof(EFI_ERROR_SECTION_DESCRIPTOR);

// Build a CPER event payload with a single section descriptor. The section body
// is `sectionBytes` of zeroes placed right after the descriptor. The descriptor
// advertises `secOffset`/`secLen` (relative to the CPER record, i.e. after the
// 4-byte common wrapper) so tests can request in- or out-of-bounds sections.
std::vector<uint8_t> makeRecord(uint16_t sectionCount, uint32_t secOffset,
                                uint32_t secLen, size_t sectionBytes)
{
    std::vector<uint8_t> buf(kCommon + kHdr + kDesc + sectionBytes, 0);

    EFI_COMMON_ERROR_RECORD_HEADER hdr{};
    hdr.SectionCount = sectionCount;
    std::memcpy(&buf[kCommon], &hdr, kHdr);

    EFI_ERROR_SECTION_DESCRIPTOR desc{};
    desc.SectionOffset = secOffset;
    desc.SectionLength = secLen;
    // SectionType is left as an all-zero GUID => "not supported" section, so no
    // sub-decoder writes to the header.
    std::memcpy(&buf[kCommon + kHdr], &desc, kDesc);

    return buf;
}

// A sentinel header. decodeCperRecord must leave it untouched unless a *known*
// in-bounds section is decoded (none of these tests supply one), which lets the
// tests assert that a malformed record is rejected rather than acted upon.
EFI_AMPERE_ERROR_DATA sentinel()
{
    EFI_AMPERE_ERROR_DATA h{};
    h.TypeId = 0xAAAA;
    h.SubtypeId = 0xBBBB;
    return h;
}

} // namespace

TEST(CperBounds, TruncatedBelowHeaderIsRejected)
{
    // eventDataSize smaller than common wrapper + record header => early
    // return.
    std::vector<uint8_t> buf(kCommon + kHdr - 1, 0);
    auto h = sentinel();
    decodeCperRecord(buf.data(), buf.size(), &h);
    EXPECT_EQ(h.TypeId, 0xAAAA);
    EXPECT_EQ(h.SubtypeId, 0xBBBB);
}

TEST(CperBounds, SectionCountBeyondBufferIsRejected)
{
    // Header claims a section count whose descriptor array cannot fit.
    auto buf = makeRecord(/*sectionCount=*/0xFFFF, kHdr + kDesc, 8, 8);
    auto h = sentinel();
    decodeCperRecord(buf.data(), buf.size(), &h);
    EXPECT_EQ(h.TypeId, 0xAAAA);
    EXPECT_EQ(h.SubtypeId, 0xBBBB);
}

TEST(CperBounds, SectionOffsetLengthOutOfBoundsIsSkipped)
{
    // One descriptor whose offset+length runs past the end of the event data.
    auto buf = makeRecord(1, /*secOffset=*/kHdr + kDesc, /*secLen=*/0x10000, 8);
    auto h = sentinel();
    decodeCperRecord(buf.data(), buf.size(), &h);
    EXPECT_EQ(h.TypeId, 0xAAAA);
    EXPECT_EQ(h.SubtypeId, 0xBBBB);
}

TEST(CperBounds, WellFormedUnknownSectionIsHandledInBounds)
{
    // A fully in-bounds section with an unknown GUID: copied safely and
    // reported as unsupported, leaving the header untouched. Exercises the
    // in-bounds memcpy path (valgrind-clean under CI).
    const size_t sectionBytes = 16;
    auto buf = makeRecord(1, /*secOffset=*/kHdr + kDesc,
                          /*secLen=*/sectionBytes, sectionBytes);
    auto h = sentinel();
    decodeCperRecord(buf.data(), buf.size(), &h);
    EXPECT_EQ(h.TypeId, 0xAAAA);
    EXPECT_EQ(h.SubtypeId, 0xBBBB);
}
