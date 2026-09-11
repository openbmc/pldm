#include "pldmtool/pldm_cmd_helper.hpp"

#include <libpldm/platform.h>

#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

using namespace pldmtool::helper;

TEST(AddRangeField, signedFormatsKeepSign)
{
    ordered_json output;
    union_range_field_format value{};

    value.value_s8 = -5;
    addRangeField(output, "sint8", PLDM_RANGE_FIELD_FORMAT_SINT8, value);
    ASSERT_TRUE(output["sint8"].is_number_integer());
    EXPECT_EQ(output["sint8"].get<int64_t>(), -5);

    value.value_s16 = -300;
    addRangeField(output, "sint16", PLDM_RANGE_FIELD_FORMAT_SINT16, value);
    ASSERT_TRUE(output["sint16"].is_number_integer());
    EXPECT_EQ(output["sint16"].get<int64_t>(), -300);

    value.value_s32 = std::numeric_limits<int32_t>::min();
    addRangeField(output, "sint32", PLDM_RANGE_FIELD_FORMAT_SINT32, value);
    ASSERT_TRUE(output["sint32"].is_number_integer());
    EXPECT_EQ(output["sint32"].get<int64_t>(),
              std::numeric_limits<int32_t>::min());
}

TEST(AddRangeField, unsignedFormatsUseFullRange)
{
    ordered_json output;
    union_range_field_format value{};

    value.value_u8 = 200;
    addRangeField(output, "uint8", PLDM_RANGE_FIELD_FORMAT_UINT8, value);
    ASSERT_TRUE(output["uint8"].is_number_integer());
    EXPECT_EQ(output["uint8"].get<int64_t>(), 200);

    value.value_u32 = std::numeric_limits<uint32_t>::max();
    addRangeField(output, "uint32", PLDM_RANGE_FIELD_FORMAT_UINT32, value);
    ASSERT_TRUE(output["uint32"].is_number_integer());
    EXPECT_EQ(output["uint32"].get<int64_t>(),
              std::numeric_limits<uint32_t>::max());
}

TEST(AddRangeField, real32KeepsFraction)
{
    ordered_json output;
    union_range_field_format value{};

    value.value_f32 = -12.5f;
    addRangeField(output, "real32", PLDM_RANGE_FIELD_FORMAT_REAL32, value);
    ASSERT_TRUE(output["real32"].is_number_float());
    EXPECT_DOUBLE_EQ(output["real32"].get<double>(), -12.5);
}

TEST(AddRangeField, undefinedFormatIsLeftOut)
{
    ordered_json output;
    union_range_field_format value{};
    value.value_u32 = 1;

    addRangeField(output, "undefined",
                  static_cast<uint8_t>(PLDM_RANGE_FIELD_FORMAT_REAL32 + 1),
                  value);
    addRangeField(output, "outOfRange", 0xFF, value);
    EXPECT_FALSE(output.contains("undefined"));
    EXPECT_FALSE(output.contains("outOfRange"));
}
