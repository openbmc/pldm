#include "../cper.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <vector>

namespace
{
using namespace pldm::oem_ampere;

constexpr size_t cperHeaderOffset = sizeof(CommonEventData);

std::vector<uint8_t> makeRecord(const EFI_GUID& sectionType,
                                size_t sectionSize)
{
    const size_t descriptorOffset =
        sizeof(EFI_COMMON_ERROR_RECORD_HEADER) +
        sizeof(EFI_ERROR_SECTION_DESCRIPTOR);
    const size_t sectionOffset = cperHeaderOffset + descriptorOffset;
    std::vector<uint8_t> data(sectionOffset + sectionSize);

    EFI_COMMON_ERROR_RECORD_HEADER header{};
    header.SectionCount = 1;
    std::memcpy(data.data() + cperHeaderOffset, &header, sizeof(header));

    EFI_ERROR_SECTION_DESCRIPTOR descriptor{};
    descriptor.SectionOffset = descriptorOffset;
    descriptor.SectionLength = sectionSize;
    std::memcpy(&descriptor.SectionType, &sectionType,
                sizeof(descriptor.SectionType));
    std::memcpy(data.data() + cperHeaderOffset +
                    sizeof(EFI_COMMON_ERROR_RECORD_HEADER),
                &descriptor, sizeof(descriptor));
    return data;
}

} // namespace

TEST(Cper, RejectsNullAndTruncatedInput)
{
    EFI_AMPERE_ERROR_DATA output{};
    EXPECT_FALSE(decodeCperRecord(nullptr, 0, &output));

    std::vector<uint8_t> data(sizeof(CommonEventData) +
                              sizeof(EFI_COMMON_ERROR_RECORD_HEADER) - 1);
    EXPECT_FALSE(decodeCperRecord(data.data(), data.size(), &output));
}

TEST(Cper, RejectsSectionDescriptorCountThatDoesNotFit)
{
    std::vector<uint8_t> data(sizeof(CommonEventData) +
                              sizeof(EFI_COMMON_ERROR_RECORD_HEADER));
    EFI_COMMON_ERROR_RECORD_HEADER header{};
    header.SectionCount =
        std::numeric_limits<decltype(header.SectionCount)>::max();
    std::memcpy(data.data() + cperHeaderOffset, &header, sizeof(header));

    EFI_AMPERE_ERROR_DATA output{};
    EXPECT_FALSE(decodeCperRecord(data.data(), data.size(), &output));
}

TEST(Cper, RejectsSectionOutsideEvent)
{
    auto data = makeRecord(gEfiAmpereErrorSectionGuid,
                           sizeof(EFI_AMPERE_ERROR_DATA));
    EFI_ERROR_SECTION_DESCRIPTOR descriptor{};
    std::memcpy(&descriptor,
                data.data() + cperHeaderOffset +
                    sizeof(EFI_COMMON_ERROR_RECORD_HEADER),
                sizeof(descriptor));
    descriptor.SectionOffset =
        std::numeric_limits<decltype(descriptor.SectionOffset)>::max();
    std::memcpy(data.data() + cperHeaderOffset +
                    sizeof(EFI_COMMON_ERROR_RECORD_HEADER),
                &descriptor, sizeof(descriptor));

    EFI_AMPERE_ERROR_DATA output{};
    EXPECT_FALSE(decodeCperRecord(data.data(), data.size(), &output));
}

TEST(Cper, RejectsTruncatedArmContext)
{
    auto data = makeRecord(gEfiArmProcessorErrorSectionGuid,
                           sizeof(EFI_ARM_ERROR_RECORD));
    EFI_ARM_ERROR_RECORD record{};
    record.SectionLength = sizeof(EFI_ARM_ERROR_RECORD);
    record.ContextInfoNum = 1;
    std::memcpy(data.data() + cperHeaderOffset +
                    sizeof(EFI_COMMON_ERROR_RECORD_HEADER) +
                    sizeof(EFI_ERROR_SECTION_DESCRIPTOR),
                &record, sizeof(record));

    EFI_AMPERE_ERROR_DATA output{};
    EXPECT_FALSE(decodeCperRecord(data.data(), data.size(), &output));
}

TEST(Cper, RejectsArmRegisterArrayPastSection)
{
    const size_t sectionSize = sizeof(EFI_ARM_ERROR_RECORD) +
                               sizeof(EFI_ARM_CONTEXT_INFORMATION_HEADER);
    auto data = makeRecord(gEfiArmProcessorErrorSectionGuid, sectionSize);

    EFI_ARM_ERROR_RECORD record{};
    record.SectionLength = sectionSize;
    record.ContextInfoNum = 1;
    std::memcpy(data.data() + cperHeaderOffset +
                    sizeof(EFI_COMMON_ERROR_RECORD_HEADER) +
                    sizeof(EFI_ERROR_SECTION_DESCRIPTOR),
                &record, sizeof(record));

    EFI_ARM_CONTEXT_INFORMATION_HEADER contextInfo{};
    contextInfo.RegisterArraySize = 1;
    std::memcpy(data.data() + cperHeaderOffset +
                    sizeof(EFI_COMMON_ERROR_RECORD_HEADER) +
                    sizeof(EFI_ERROR_SECTION_DESCRIPTOR) +
                    sizeof(EFI_ARM_ERROR_RECORD),
                &contextInfo, sizeof(contextInfo));

    EFI_AMPERE_ERROR_DATA output{};
    EXPECT_FALSE(decodeCperRecord(data.data(), data.size(), &output));
}

TEST(Cper, AcceptsMinimalValidAmpereRecord)
{
    auto data = makeRecord(gEfiAmpereErrorSectionGuid,
                           sizeof(EFI_AMPERE_ERROR_DATA));
    const EFI_AMPERE_ERROR_DATA expected{
        .TypeId = 0x1234,
        .SubtypeId = 0x5678,
        .InstanceId = 0x9abcdef0,
    };
    std::memcpy(data.data() + cperHeaderOffset +
                    sizeof(EFI_COMMON_ERROR_RECORD_HEADER) +
                    sizeof(EFI_ERROR_SECTION_DESCRIPTOR),
                &expected, sizeof(expected));

    EFI_AMPERE_ERROR_DATA output{};
    EXPECT_TRUE(decodeCperRecord(data.data(), data.size(), &output));
    EXPECT_EQ(output.TypeId, expected.TypeId);
    EXPECT_EQ(output.SubtypeId, expected.SubtypeId);
    EXPECT_EQ(output.InstanceId, expected.InstanceId);
}
