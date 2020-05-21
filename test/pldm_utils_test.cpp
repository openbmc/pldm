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

TEST(parseEffecterData, testGoodDecodeEffecterData)
{
    std::vector<uint8_t> effecterData = {1, 1, 0, 1};
    uint8_t effecterCount = 2;
    set_effecter_state_field stateField0 = {1, 1};
    set_effecter_state_field stateField1 = {0, 1};

    auto effecterField = parseEffecterData(effecterData, effecterCount);
    EXPECT_NE(effecterField, std::nullopt);
    EXPECT_EQ(effecterCount, effecterField->size());

    std::vector<set_effecter_state_field> stateField = effecterField.value();
    EXPECT_EQ(stateField[0].set_request, stateField0.set_request);
    EXPECT_EQ(stateField[0].effecter_state, stateField0.effecter_state);
    EXPECT_EQ(stateField[1].set_request, stateField1.set_request);
    EXPECT_EQ(stateField[1].effecter_state, stateField1.effecter_state);
}

TEST(parseEffecterData, testBadDecodeEffecterData)
{
    std::vector<uint8_t> effecterData = {0, 1, 0, 1, 0, 1};
    uint8_t effecterCount = 2;

    auto effecterField = parseEffecterData(effecterData, effecterCount);

    EXPECT_EQ(effecterField, std::nullopt);
}

TEST(parseStateEffecterData, testGoodResponse)
{
    MockPldmSendRecv sendRecv;
    auto repo = pldm_pdr_init();
    uint16_t entityType = 67;
    uint16_t stateSetId = 192;
    uint8_t compositeEffecterCount = 1;
    uint16_t effecterID = 21;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states));
    auto record = reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());
    auto possibleStates =
        reinterpret_cast<state_effecter_possible_states*>(pdr->possible_states);
    std::vector<set_effecter_state_field> stateField(
        compositeEffecterCount * sizeof(set_effecter_state_field));

    record->hdr.type = PLDM_STATE_EFFECTER_PDR;
    record->hdr.record_handle = 1;
    record->entity_type = entityType;
    record->container_id = 0;
    record->composite_effecter_count = compositeEffecterCount;
    record->effecter_id = effecterID;
    possibleStates->state_set_id = stateSetId;
    stateField.emplace_back({PLDM_REQUEST_SET, 3});
    uint8_t pldm_requester_rc;

    pldm_pdr_add(repo, pdr.data(), pdr.size(), 0, false);

    std::vector<uint8_t> stateEffecReqMsg(
        sizeof(pldm_msg_hdr) + sizeof(effecterID) +
        sizeof(compositeEffecterCount) + stateField.size());
    auto stateEffecReq = reinterpret_cast<pldm_msg*>(stateEffecReqMsg.data());

    auto rc = encode_set_state_effecter_states_req(
        0, effecterID, compositeEffecterCount, stateField.data(),
        stateEffecReq);

    EXPECT_EQ(rc, PLDM_SUCCESS);

    uint8_t* pdrResponseMsg = nullptr;
    size_t pdrResponseMsgSize{};

    EXPECT_CALL(sendRecv, pldm_send_recv(0, 1, stateEffecReqMsg.data(),
                                         stateEffecReqMsg.size(),
                                         &pdrResponseMsg, &pdrResponseMsgSize))
        .WillOnce(Return(pldm_requester_rc(0)));
}
