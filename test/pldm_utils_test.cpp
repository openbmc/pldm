#include "utils.hpp"

#include <gtest/gtest.h>

using namespace pldm::utils;

TEST(decodeDate, testGooduintToDate)
{
    uint64_t data = 20191212115959;
    uint16_t year = 2019;
    uint8_t month = 12;
    uint8_t day = 12;
    uint8_t hours = 11;
    uint8_t minutes = 59;
    uint8_t seconds = 59;

    uint16_t retyear = 0;
    uint8_t retmonth = 0;
    uint8_t retday = 0;
    uint8_t rethours = 0;
    uint8_t retminutes = 0;
    uint8_t retseconds = 0;

    auto ret = uintToDate(data, &retyear, &retmonth, &retday, &rethours,
                          &retminutes, &retseconds);

    EXPECT_EQ(ret, true);
    EXPECT_EQ(year, retyear);
    EXPECT_EQ(month, retmonth);
    EXPECT_EQ(day, retday);
    EXPECT_EQ(hours, rethours);
    EXPECT_EQ(minutes, retminutes);
    EXPECT_EQ(seconds, retseconds);
}

TEST(decodeDate, testBaduintToDate)
{
    uint64_t data = 10191212115959;

    uint16_t retyear = 0;
    uint8_t retmonth = 0;
    uint8_t retday = 0;
    uint8_t rethours = 0;
    uint8_t retminutes = 0;
    uint8_t retseconds = 0;

    auto ret = uintToDate(data, &retyear, &retmonth, &retday, &rethours,
                          &retminutes, &retseconds);

    EXPECT_EQ(ret, false);
}

TEST(decodeEffecterData, testGoodDecodeEffecterData)
{
    std::vector<uint8_t> effecterData = {2, 1, 1, 0, 1, 2};
    uint16_t effecter_id = 2;
    set_effecter_state_field stateField0 = {1, 1};
    set_effecter_state_field stateField1 = {0, 0};
    set_effecter_state_field stateField2 = {1, 2};

    uint16_t retEffecter_id = 0;
    std::vector<set_effecter_state_field> stateField = {};
    auto rc = decodeEffecterData(effecterData, retEffecter_id, stateField);

    EXPECT_EQ(rc, true);
    EXPECT_EQ(effecter_id, retEffecter_id);
    EXPECT_EQ(stateField[0].set_request, stateField0.set_request);
    EXPECT_EQ(stateField[0].effecter_state, stateField0.effecter_state);
    EXPECT_EQ(stateField[1].set_request, stateField1.set_request);
    EXPECT_EQ(stateField[1].effecter_state, stateField1.effecter_state);
    EXPECT_EQ(stateField[2].set_request, stateField2.set_request);
    EXPECT_EQ(stateField[2].effecter_state, stateField2.effecter_state);
}

TEST(decodeEffecterData, testBadDecodeEffecterData)
{
    std::vector<uint8_t> effecterData = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    uint16_t retEffecter_id = 0;
    std::vector<set_effecter_state_field> stateField = {};
    auto rc = decodeEffecterData(effecterData, retEffecter_id, stateField);

    EXPECT_EQ(rc, false);
}
