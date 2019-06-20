#include "libpldmresponder/bios.hpp"
#include "libpldmresponder/bios_parser.hpp"

#include <string.h>

#include <array>
#include <ctime>

#include "libpldm/base.h"
#include "libpldm/bios.h"

#include <gtest/gtest.h>

using namespace pldm::responder;
using namespace pldm::responder::utils;

TEST(epochToBCDTime, testTime)
{
    struct tm time
    {
    };
    time.tm_year = 119;
    time.tm_mon = 3;
    time.tm_mday = 13;
    time.tm_hour = 5;
    time.tm_min = 18;
    time.tm_sec = 13;
    time.tm_isdst = -1;

    time_t epochTime = mktime(&time);
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;

    epochToBCDTime(epochTime, seconds, minutes, hours, day, month, year);

    ASSERT_EQ(0x13, seconds);
    ASSERT_EQ(0x18, minutes);
    ASSERT_EQ(0x5, hours);
    ASSERT_EQ(0x13, day);
    ASSERT_EQ(0x4, month);
    ASSERT_EQ(0x2019, year);
}

TEST(GetBIOSStrings, allScenarios)
{
    using namespace bios_parser;
    // All the BIOS Strings in the BIOS JSON config files.
    Strings vec{"HMCManagedState",  "On",         "Off",
                "FWBootSide",       "Perm",       "Temp",
                "CodeUpdatePolicy", "Concurrent", "Disruptive"};

    Strings nullVec{};

    // Invalid directory
    auto strings = bios_parser::getStrings("./bios_json");
    ASSERT_EQ(strings == nullVec, true);

    strings = bios_parser::getStrings("./bios_jsons");
    ASSERT_EQ(strings == vec, true);
}

TEST(getAttrValue, allScenarios)
{
    using namespace bios_parser::bios_enum;
    // All the BIOS Strings in the BIOS JSON config files.
    AttrValuesMap valueMap{
        {"HMCManagedState", {false, {"On", "Off"}, {"On"}}},
        {"FWBootSide", {false, {"Perm", "Temp"}, {"Perm"}}},
        {"CodeUpdatePolicy",
         {false, {"Concurrent", "Disruptive"}, {"Concurrent"}}}};

    auto rc = setupValueLookup("./bios_jsons");
    ASSERT_EQ(rc, 0);

    auto values = getValues();
    ASSERT_EQ(valueMap == values, true);

    CurrentValues cv{"Concurrent"};
    auto value = getAttrValue("CodeUpdatePolicy");
    ASSERT_EQ(value == cv, true);

    // Invalid attribute name
    ASSERT_THROW(getAttrValue("CodeUpdatePolic"), std::out_of_range);
}
