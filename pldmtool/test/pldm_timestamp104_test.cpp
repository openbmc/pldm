#include "pldm_timestamp104.hpp"

#include <libpldm/base.h>

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using pldmtool::formatTimestamp104;

namespace
{

// DSP0240 v1.2.0 section 2.5 byte 12 bits [3:0]
enum TimeResolution : uint8_t
{
    microsecond = 0,
    tenMicroseconds = 1,
    hundredMicroseconds = 2,
    millisecond = 3,
    tenMilliseconds = 4,
    hundredMilliseconds = 5,
    second = 6,
    tenSeconds = 7,
    minute = 8,
    tenMinutes = 9,
    hour = 10,
    day = 11,
    month = 12,
    year = 13,
    reservedTime = 14,
    null = 15,
};

// DSP0240 v1.2.0 section 2.5 byte 12 bits [7:4]
enum UtcResolution : uint8_t
{
    unspecified = 0,
    utcMinute = 1,
    utcTenMinutes = 2,
    utcHour = 3,
    reservedUtc = 4,
};

struct Timestamp104
{
    int16_t utcOffsetMinutes = 330;
    uint32_t microseconds = 123456;
    uint8_t seconds = 45;
    uint8_t minutes = 30;
    uint8_t hours = 15;
    uint8_t dayOfMonth = 20;
    uint8_t monthOfYear = 8;
    uint16_t yearValue = 2026;
    uint8_t utcResolution = utcMinute;
    uint8_t timeResolution = microsecond;

    std::vector<uint8_t> encode() const
    {
        const auto offset = static_cast<uint16_t>(utcOffsetMinutes);
        return {
            static_cast<uint8_t>(offset),
            static_cast<uint8_t>(offset >> 8),
            static_cast<uint8_t>(microseconds),
            static_cast<uint8_t>(microseconds >> 8),
            static_cast<uint8_t>(microseconds >> 16),
            seconds,
            minutes,
            hours,
            dayOfMonth,
            monthOfYear,
            static_cast<uint8_t>(yearValue),
            static_cast<uint8_t>(yearValue >> 8),
            static_cast<uint8_t>((utcResolution << 4) | timeResolution),
        };
    }
};

std::string format(const Timestamp104& ts)
{
    return formatTimestamp104(ts.encode());
}

} // namespace

TEST(FormatTimestamp104, FullPrecisionWithPositiveOffset)
{
    EXPECT_EQ(format({}), "2026-08-20T15:30:45.123456+05:30");
}

TEST(FormatTimestamp104, NegativeOffset)
{
    EXPECT_EQ(format({.utcOffsetMinutes = -480}),
              "2026-08-20T15:30:45.123456-08:00");
}

TEST(FormatTimestamp104, UnspecifiedUtcOmitsOffset)
{
    EXPECT_EQ(format({.utcResolution = unspecified}),
              "2026-08-20T15:30:45.123456");
}

TEST(FormatTimestamp104, UtcResolution)
{
    EXPECT_EQ(format({.utcResolution = utcHour}),
              "2026-08-20T15:30:45.123456+05");
    EXPECT_EQ(format({.utcResolution = utcTenMinutes}),
              "2026-08-20T15:30:45.123456+05:3*");
    EXPECT_EQ(format({.utcOffsetMinutes = -480, .utcResolution = utcHour}),
              "2026-08-20T15:30:45.123456-08");
}

TEST(FormatTimestamp104, TimeResolution)
{
    const struct
    {
        uint8_t resolution;
        const char* expected;
    } cases[] = {
        {microsecond, "2026-08-20T15:30:45.123456"},
        {tenMicroseconds, "2026-08-20T15:30:45.12345"},
        {hundredMicroseconds, "2026-08-20T15:30:45.1234"},
        {millisecond, "2026-08-20T15:30:45.123"},
        {tenMilliseconds, "2026-08-20T15:30:45.12"},
        {hundredMilliseconds, "2026-08-20T15:30:45.1"},
        {second, "2026-08-20T15:30:45"},
        {tenSeconds, "2026-08-20T15:30:4*"},
        {minute, "2026-08-20T15:30"},
        {tenMinutes, "2026-08-20T15:3*"},
        {hour, "2026-08-20T15"},
        {day, "2026-08-20"},
        {month, "2026-08"},
        {year, "2026"},
    };

    for (const auto& [resolution, expected] : cases)
    {
        EXPECT_EQ(format({.utcResolution = unspecified,
                          .timeResolution = resolution}),
                  expected)
            << "time resolution " << static_cast<int>(resolution);
    }
}

TEST(FormatTimestamp104, YearResolutionIgnoresZeroedFields)
{
    EXPECT_EQ(format({.utcOffsetMinutes = 0,
                      .microseconds = 0,
                      .seconds = 0,
                      .minutes = 0,
                      .hours = 0,
                      .dayOfMonth = 0,
                      .monthOfYear = 0,
                      .utcResolution = unspecified,
                      .timeResolution = year}),
              "2026");
}

TEST(FormatTimestamp104, CoarseTimeKeepsOffset)
{
    EXPECT_EQ(format({.timeResolution = day}), "2026-08-20+05:30");
}

TEST(FormatTimestamp104, NullResolutionIsUnknown)
{
    EXPECT_EQ(format({.timeResolution = null}), "<unknown>");
}

TEST(FormatTimestamp104, ReservedResolutionIsMalformed)
{
    EXPECT_EQ(format({.timeResolution = reservedTime}),
              "<malformed: reserved time resolution 14>");
    EXPECT_EQ(format({.utcResolution = reservedUtc}),
              "<malformed: reserved UTC resolution 4>");
}

TEST(FormatTimestamp104, InvalidLengthIsMalformed)
{
    auto bytes = Timestamp104{}.encode();
    bytes.pop_back();
    EXPECT_EQ(formatTimestamp104(bytes),
              "<malformed: expected 13 bytes, got 12>");

    bytes.resize(PLDM_TIMESTAMP104_SIZE + 1);
    EXPECT_EQ(formatTimestamp104(bytes),
              "<malformed: expected 13 bytes, got 14>");

    EXPECT_EQ(formatTimestamp104({}), "<malformed: expected 13 bytes, got 0>");
}
