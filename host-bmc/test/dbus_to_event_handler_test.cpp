#include "../dbus_to_event_handler.hpp"

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

using namespace pldm::state_sensor;
using namespace pldm::utils;

TEST(IsValueMatched, StringValuesMatchOneOfPipeSeparatedList)
{
    PropertyValue expected{std::string("Enabled||Active")};
    PropertyValue actual{std::string("Active")};
    EXPECT_TRUE(isValueMatched("string", expected, actual));
}

TEST(IsValueMatched, StringValuesNoMatch)
{
    PropertyValue expected{std::string("Enabled||Active")};
    PropertyValue actual{std::string("Disabled")};
    EXPECT_FALSE(isValueMatched("string", expected, actual));
}

TEST(IsValueMatched, StringTypeMismatchNoThrow)
{
    PropertyValue expected{std::string("Enabled")};
    PropertyValue actual{static_cast<int32_t>(1)};
    EXPECT_NO_THROW({
        EXPECT_FALSE(isValueMatched("string", expected, actual));
    });
}

TEST(IsValueMatched, StringTypeMismatchOnExpectedSideReturnsFalse)
{
    PropertyValue expected{static_cast<uint32_t>(1)};
    PropertyValue actual{std::string("Enabled")};
    EXPECT_NO_THROW({
        EXPECT_FALSE(isValueMatched("string", expected, actual));
    });
}

TEST(IsValueMatched, NonStringValuesMatch)
{
    PropertyValue expected{static_cast<uint32_t>(42)};
    PropertyValue actual{static_cast<uint32_t>(42)};
    EXPECT_TRUE(isValueMatched("uint32_t", expected, actual));
}

TEST(IsValueMatched, NonStringValuesNoMatch)
{
    PropertyValue expected{static_cast<uint32_t>(42)};
    PropertyValue actual{static_cast<uint32_t>(7)};
    EXPECT_FALSE(isValueMatched("uint32_t", expected, actual));
}

TEST(IsValueMatched, NonStringTypeMismatchNoThrow)
{
    PropertyValue expected{static_cast<uint32_t>(42)};
    PropertyValue actual{std::string("42")};
    EXPECT_NO_THROW({
        EXPECT_FALSE(isValueMatched("uint32_t", expected, actual));
    });
}

TEST(IsValueMatched, StringValuesMatchWithWhitespaceAroundPipe)
{
    PropertyValue expected{std::string("Enabled || Active")};
    PropertyValue actual{std::string("Active")};
    EXPECT_TRUE(isValueMatched("string", expected, actual));
}

TEST(IsValueMatched, EmptyExpectedStringNoMatch)
{
    PropertyValue expected{std::string("")};
    PropertyValue actual{std::string("Active")};
    EXPECT_FALSE(isValueMatched("string", expected, actual));
}
