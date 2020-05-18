#include "utils.hpp"

#include "libpldm/platform.h"

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

TEST(FindStateEffecterPDR, testOneMatch)
{

    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 33;
    uint16_t stateSetId = 196;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states));

    auto rec = reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_effecter_possible_states*>(rec->possible_states);

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 33;
    rec->container_id = 0;
    state->state_set_id = 196;
    state->possible_states_size = 1;

    pldm_pdr_add(repo, pdr.data(), pdr.size(), 0, false);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr, record[0]);
}

TEST(FindStateEffecterPDR, testNoMatch)
{

    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 44;
    uint16_t stateSetId = 196;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states));

    auto rec = reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_effecter_possible_states*>(rec->possible_states);

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 33;
    rec->container_id = 0;
    state->state_set_id = 196;
    state->possible_states_size = 1;

    pldm_pdr_add(repo, pdr.data(), pdr.size(), 0, false);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);
}

TEST(FindStateEffecterPDR, testnoMatch)
{

    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 33;
    uint16_t stateSetId = 196;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states));

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);
}

TEST(FindStateEffecterPDR, testMoreMatch)
{

    auto repo = pldm_pdr_init();
    uint8_t tid = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states));

    auto rec = reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_effecter_possible_states*>(rec->possible_states);

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 31;
    rec->container_id = 0;
    state->state_set_id = 129;
    state->possible_states_size = 1;

    pldm_pdr_add(repo, pdr.data(), pdr.size(), 0, false);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_ = reinterpret_cast<pldm_state_effecter_pdr*>(pdr_second.data());

    auto state_ = reinterpret_cast<state_effecter_possible_states*>(
        rec_->possible_states);

    rec_->hdr.type = 11;
    rec_->hdr.record_handle = 2;
    rec_->entity_type = 31;
    rec_->container_id = 0;
    state_->state_set_id = 129;
    state_->possible_states_size = 1;

    pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), 0, false);

    uint16_t entityID_ = 31;
    uint16_t stateSetId_ = 129;

    auto record = findStateEffecterPDR(tid, entityID_, stateSetId_, repo);

    EXPECT_EQ(pdr, record[0]);
    EXPECT_EQ(pdr_second, record[1]);
}
TEST(FindStateEffecterPDR, testManyNoMatch)
{

    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 33;
    uint16_t stateSetId = 196;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states));

    auto rec = reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_effecter_possible_states*>(rec->possible_states);

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 34;
    rec->container_id = 0;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    pldm_pdr_add(repo, pdr.data(), pdr.size(), 0, false);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_ = reinterpret_cast<pldm_state_effecter_pdr*>(pdr_second.data());

    auto state_ = reinterpret_cast<state_effecter_possible_states*>(
        rec_->possible_states);

    rec_->hdr.type = 11;
    rec_->hdr.record_handle = 2;
    rec_->entity_type = 39;
    rec_->container_id = 0;
    state_->state_set_id = 169;
    state_->possible_states_size = 1;

    pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), 0, false);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);
}

TEST(FindStateEffecterPDR, testOneMatchOneNoMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 67;
    uint16_t stateSetId = 192;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states));

    auto rec = reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_effecter_possible_states*>(rec->possible_states);

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 32;
    rec->container_id = 0;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    pldm_pdr_add(repo, pdr.data(), pdr.size(), 0, false);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_ = reinterpret_cast<pldm_state_effecter_pdr*>(pdr_second.data());

    auto state_ = reinterpret_cast<state_effecter_possible_states*>(
        rec_->possible_states);

    rec_->hdr.type = 11;
    rec_->hdr.record_handle = 2;
    rec_->entity_type = 67;
    rec_->container_id = 0;
    state_->state_set_id = 192;
    state_->possible_states_size = 1;

    pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), 0, false);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr_second, record[0]);
}

TEST(FindStateEffecterPDR, testOneMatchManyNoMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 67;
    uint16_t stateSetId = 192;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states));

    auto rec = reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_effecter_possible_states*>(rec->possible_states);

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 32;
    rec->container_id = 0;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    pldm_pdr_add(repo, pdr.data(), pdr.size(), 0, false);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_ = reinterpret_cast<pldm_state_effecter_pdr*>(pdr_second.data());

    auto state_ = reinterpret_cast<state_effecter_possible_states*>(
        rec_->possible_states);

    rec_->hdr.type = 11;
    rec_->hdr.record_handle = 2;
    rec_->entity_type = 67;
    rec_->container_id = 0;
    state_->state_set_id = 192;
    state_->possible_states_size = 1;

    pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), 0, false);

    std::vector<uint8_t> pdr_third(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_third =
        reinterpret_cast<pldm_state_effecter_pdr*>(pdr_third.data());

    auto state_third = reinterpret_cast<state_effecter_possible_states*>(
        rec_third->possible_states);

    rec_third->hdr.type = 11;
    rec_third->hdr.record_handle = 3;
    rec_third->entity_type = 69;
    rec_third->container_id = 0;
    state_third->state_set_id = 199;
    state_third->possible_states_size = 1;

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr_second, record[0]);
}
