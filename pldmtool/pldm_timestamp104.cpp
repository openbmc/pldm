#include "pldm_timestamp104.hpp"

#include <endian.h>
#include <libpldm/base.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <format>

namespace pldmtool
{

namespace
{

// timestamp104 layout, DSP0240 v1.2.0 section 2.5
constexpr size_t utcOffsetPos = 0;
constexpr size_t microsecondPos = 2;
constexpr size_t secondPos = 5;
constexpr size_t minutePos = 6;
constexpr size_t hourPos = 7;
constexpr size_t dayPos = 8;
constexpr size_t monthPos = 9;
constexpr size_t yearPos = 10;
constexpr size_t resolutionPos = 12;

constexpr uint8_t timeResolutionMask = 0x0F;
constexpr uint8_t timeResolution10Second = 7;
constexpr uint8_t timeResolution10Minute = 9;
constexpr uint8_t timeResolutionNull = 15;

constexpr uint8_t utcResolutionShift = 4;
constexpr uint8_t utcResolutionUnspecified = 0;
constexpr uint8_t utcResolution10Minute = 2;
constexpr uint8_t utcResolutionHour = 3;

// Significant prefix of "YYYY-MM-DDTHH:MM:SS.ffffff" per time resolution
constexpr std::array<size_t, 14> timeSignificantChars{
    26, // microsecond
    25, // 10 microsecond
    24, // 100 microsecond
    23, // millisecond
    22, // 10 millisecond
    21, // 100 millisecond
    19, // second
    18, // 10 second
    16, // minute
    15, // 10 minute
    13, // hour
    10, // day
    7,  // month
    4,  // year
};

constexpr char partialFieldMarker = '*';

} // namespace

std::string formatTimestamp104(std::span<const uint8_t> value)
{
    if (value.size() != PLDM_TIMESTAMP104_SIZE)
    {
        return std::format("<malformed: expected {} bytes, got {}>",
                           PLDM_TIMESTAMP104_SIZE, value.size());
    }

    const uint8_t timeResolution = value[resolutionPos] & timeResolutionMask;
    const uint8_t utcResolution = value[resolutionPos] >> utcResolutionShift;

    if (timeResolution == timeResolutionNull)
    {
        return "<unknown>";
    }
    if (timeResolution >= timeSignificantChars.size())
    {
        return std::format("<malformed: reserved time resolution {}>",
                           timeResolution);
    }
    if (utcResolution > utcResolutionHour)
    {
        return std::format("<malformed: reserved UTC resolution {}>",
                           utcResolution);
    }

    uint16_t year = 0;
    std::memcpy(&year, value.data() + yearPos, sizeof(year));
    year = le16toh(year);

    const uint32_t microsecond =
        static_cast<uint32_t>(value[microsecondPos]) |
        (static_cast<uint32_t>(value[microsecondPos + 1]) << 8) |
        (static_cast<uint32_t>(value[microsecondPos + 2]) << 16);

    std::string datetime =
        std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:06}", year,
                    value[monthPos], value[dayPos], value[hourPos],
                    value[minutePos], value[secondPos], microsecond);
    datetime.resize(timeSignificantChars[timeResolution]);
    if (timeResolution == timeResolution10Second ||
        timeResolution == timeResolution10Minute)
    {
        datetime += partialFieldMarker;
    }

    if (utcResolution == utcResolutionUnspecified)
    {
        return datetime;
    }

    uint16_t rawOffset = 0;
    std::memcpy(&rawOffset, value.data() + utcOffsetPos, sizeof(rawOffset));
    const auto utcOffsetMinutes = static_cast<int16_t>(le16toh(rawOffset));
    const std::chrono::hh_mm_ss offset{
        std::chrono::minutes{std::abs(utcOffsetMinutes)}};
    const char sign = utcOffsetMinutes < 0 ? '-' : '+';
    const auto hours = offset.hours().count();
    const auto minutes = offset.minutes().count();

    switch (utcResolution)
    {
        case utcResolutionHour:
            return std::format("{}{}{:02}", datetime, sign, hours);
        case utcResolution10Minute:
            return std::format("{}{}{:02}:{}{}", datetime, sign, hours,
                               minutes / 10, partialFieldMarker);
        default:
            return std::format("{}{}{:02}:{:02}", datetime, sign, hours,
                               minutes);
    }
}

} // namespace pldmtool
