#include "cper.hpp"

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

namespace pldm::oem_ampere
{
namespace
{
std::vector<uint8_t> makeRecord(uint16_t sectionCount)
{
    std::vector<uint8_t> data(
        sizeof(CommonEventData) + sizeof(EFI_COMMON_ERROR_RECORD_HEADER));
    EFI_COMMON_ERROR_RECORD_HEADER header{};
    header.SectionCount = sectionCount;
    std::memcpy(data.data() + sizeof(CommonEventData), &header, sizeof(header));
    return data;
}

TEST(DecodeCperRecord, RejectsShortHeader)
{
    EFI_AMPERE_ERROR_DATA ampere{};
    std::vector<uint8_t> data(sizeof(CommonEventData));
    EXPECT_FALSE(decodeCperRecord(data.data(), data.size(), &ampere));
}

TEST(DecodeCperRecord, RejectsTruncatedDescriptors)
{
    EFI_AMPERE_ERROR_DATA ampere{};
    auto data = makeRecord(1);
    EXPECT_FALSE(decodeCperRecord(data.data(), data.size(), &ampere));
}

TEST(DecodeCperRecord, RejectsSectionOutsideEvent)
{
    EFI_AMPERE_ERROR_DATA ampere{};
    auto data = makeRecord(1);
    EFI_ERROR_SECTION_DESCRIPTOR descriptor{};
    descriptor.SectionOffset = UINT32_MAX;
    descriptor.SectionLength = sizeof(ampere);
    descriptor.SectionType = gEfiAmpereErrorSectionGuid;
    const auto oldSize = data.size();
    data.resize(oldSize + sizeof(descriptor));
    std::memcpy(data.data() + oldSize, &descriptor, sizeof(descriptor));

    EXPECT_FALSE(decodeCperRecord(data.data(), data.size(), &ampere));
}

TEST(DecodeCperRecord, RejectsUndersizedTypedSection)
{
    EFI_AMPERE_ERROR_DATA ampere{};
    auto data = makeRecord(1);
    EFI_ERROR_SECTION_DESCRIPTOR descriptor{};
    descriptor.SectionOffset =
        data.size() + sizeof(descriptor) - sizeof(CommonEventData);
    descriptor.SectionLength = sizeof(ampere) - 1;
    descriptor.SectionType = gEfiAmpereErrorSectionGuid;
    const auto oldSize = data.size();
    data.resize(oldSize + sizeof(descriptor) + descriptor.SectionLength);
    std::memcpy(data.data() + oldSize, &descriptor, sizeof(descriptor));

    EXPECT_FALSE(decodeCperRecord(data.data(), data.size(), &ampere));
}

TEST(DecodeCperRecord, DecodesBoundedAmpereSection)
{
    EFI_AMPERE_ERROR_DATA expected{0x1234, 0x5678, 0x9abcdef0};
    EFI_AMPERE_ERROR_DATA actual{};
    auto data = makeRecord(1);
    EFI_ERROR_SECTION_DESCRIPTOR descriptor{};
    descriptor.SectionOffset =
        data.size() + sizeof(descriptor) - sizeof(CommonEventData);
    descriptor.SectionLength = sizeof(expected);
    descriptor.SectionType = gEfiAmpereErrorSectionGuid;
    const auto oldSize = data.size();
    data.resize(oldSize + sizeof(descriptor) + sizeof(expected));
    std::memcpy(data.data() + oldSize, &descriptor, sizeof(descriptor));
    std::memcpy(data.data() + oldSize + sizeof(descriptor), &expected,
                sizeof(expected));

    ASSERT_TRUE(decodeCperRecord(data.data(), data.size(), &actual));
    EXPECT_EQ(actual.TypeId, expected.TypeId);
    EXPECT_EQ(actual.SubtypeId, expected.SubtypeId);
    EXPECT_EQ(actual.InstanceId, expected.InstanceId);
}
} // namespace
} // namespace pldm::oem_ampere
