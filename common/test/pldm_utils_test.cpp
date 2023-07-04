#include "common/utils.hpp"

#include <libpldm/platform.h>

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
    rec->composite_effecter_count = 1;
    state->state_set_id = 196;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr, record[0]);

    pldm_pdr_destroy(repo);
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
    rec->composite_effecter_count = 1;
    state->state_set_id = 196;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);

    pldm_pdr_destroy(repo);
}

TEST(FindStateEffecterPDR, testEmptyRepo)
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

    pldm_pdr_destroy(repo);
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
    rec->composite_effecter_count = 1;
    state->state_set_id = 129;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_second =
        reinterpret_cast<pldm_state_effecter_pdr*>(pdr_second.data());

    auto state_second = reinterpret_cast<state_effecter_possible_states*>(
        rec_second->possible_states);

    rec_second->hdr.type = 11;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 31;
    rec_second->container_id = 0;
    rec_second->composite_effecter_count = 1;
    state_second->state_set_id = 129;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add_check(repo, pdr_second.data(), pdr_second.size(),
                                 false, 1, &handle),
              0);

    uint16_t entityID_ = 31;
    uint16_t stateSetId_ = 129;

    auto record = findStateEffecterPDR(tid, entityID_, stateSetId_, repo);

    EXPECT_EQ(pdr, record[0]);
    EXPECT_EQ(pdr_second, record[1]);

    pldm_pdr_destroy(repo);
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
    rec->composite_effecter_count = 1;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_second =
        reinterpret_cast<pldm_state_effecter_pdr*>(pdr_second.data());

    auto state_second = reinterpret_cast<state_effecter_possible_states*>(
        rec_second->possible_states);

    rec_second->hdr.type = 11;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 39;
    rec_second->container_id = 0;
    rec_second->composite_effecter_count = 1;
    state_second->state_set_id = 169;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add_check(repo, pdr_second.data(), pdr_second.size(),
                                 false, 1, &handle),
              0);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);

    pldm_pdr_destroy(repo);
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
    rec->composite_effecter_count = 1;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_second =
        reinterpret_cast<pldm_state_effecter_pdr*>(pdr_second.data());

    auto state_second = reinterpret_cast<state_effecter_possible_states*>(
        rec_second->possible_states);

    rec_second->hdr.type = 11;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 67;
    rec_second->container_id = 0;
    rec_second->composite_effecter_count = 1;
    state_second->state_set_id = 192;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add_check(repo, pdr_second.data(), pdr_second.size(),
                                 false, 1, &handle),
              0);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr_second, record[0]);
    EXPECT_EQ(record.size(), 1);

    pldm_pdr_destroy(repo);
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
    rec->composite_effecter_count = 1;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_second =
        reinterpret_cast<pldm_state_effecter_pdr*>(pdr_second.data());

    auto state_second = reinterpret_cast<state_effecter_possible_states*>(
        rec_second->possible_states);

    rec_second->hdr.type = 11;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 67;
    rec_second->container_id = 0;
    rec_second->composite_effecter_count = 1;
    state_second->state_set_id = 192;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add_check(repo, pdr_second.data(), pdr_second.size(),
                                 false, 1, &handle),
              0);

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
    rec_third->composite_effecter_count = 1;
    state_third->state_set_id = 199;
    state_third->possible_states_size = 1;

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr_second, record[0]);
    EXPECT_EQ(record.size(), 1);

    pldm_pdr_destroy(repo);
}

TEST(FindStateEffecterPDR, testCompositeEffecter)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 67;
    uint16_t stateSetId = 192;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states) * 3);

    auto rec = reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());
    auto state_start = rec->possible_states;

    auto state = reinterpret_cast<state_effecter_possible_states*>(state_start);

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 67;
    rec->container_id = 0;
    rec->composite_effecter_count = 3;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = reinterpret_cast<state_effecter_possible_states*>(state_start);
    state->state_set_id = 193;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = reinterpret_cast<state_effecter_possible_states*>(state_start);
    state->state_set_id = 192;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr, record[0]);

    pldm_pdr_destroy(repo);
}

TEST(FindStateEffecterPDR, testNoMatchCompositeEffecter)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 67;
    uint16_t stateSetId = 192;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_effecter_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_effecter_possible_states) * 3);

    auto rec = reinterpret_cast<pldm_state_effecter_pdr*>(pdr.data());
    auto state_start = rec->possible_states;

    auto state = reinterpret_cast<state_effecter_possible_states*>(state_start);

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 34;
    rec->container_id = 0;
    rec->composite_effecter_count = 3;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = reinterpret_cast<state_effecter_possible_states*>(state_start);
    state->state_set_id = 193;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = reinterpret_cast<state_effecter_possible_states*>(state_start);
    state->state_set_id = 123;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testOneMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 5;
    uint16_t stateSetId = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_sensor_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_sensor_possible_states));

    auto rec = reinterpret_cast<pldm_state_sensor_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_sensor_possible_states*>(rec->possible_states);

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 5;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 1;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    auto record = findStateSensorPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr, record[0]);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testNoMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 5;
    uint16_t stateSetId = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_sensor_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_sensor_possible_states));

    auto rec = reinterpret_cast<pldm_state_sensor_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_sensor_possible_states*>(rec->possible_states);

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 55;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 1;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    auto record = findStateSensorPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testEmptyRepo)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 5;
    uint16_t stateSetId = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_sensor_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_sensor_possible_states));

    auto record = findStateSensorPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testMoreMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_sensor_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_sensor_possible_states));

    auto rec = reinterpret_cast<pldm_state_sensor_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_sensor_possible_states*>(rec->possible_states);

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 5;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 1;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec_second =
        reinterpret_cast<pldm_state_sensor_pdr*>(pdr_second.data());

    auto state_second = reinterpret_cast<state_sensor_possible_states*>(
        rec_second->possible_states);

    rec_second->hdr.type = 4;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 5;
    rec_second->container_id = 0;
    rec_second->composite_sensor_count = 1;
    state_second->state_set_id = 1;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add_check(repo, pdr_second.data(), pdr_second.size(),
                                 false, 1, &handle),
              0);

    uint16_t entityID_ = 5;
    uint16_t stateSetId_ = 1;

    auto record = findStateSensorPDR(tid, entityID_, stateSetId_, repo);

    EXPECT_EQ(pdr, record[0]);
    EXPECT_EQ(pdr_second, record[1]);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testManyNoMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 5;
    uint16_t stateSetId = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_sensor_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_sensor_possible_states));

    auto rec = reinterpret_cast<pldm_state_sensor_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_sensor_possible_states*>(rec->possible_states);

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 56;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 2;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec_second =
        reinterpret_cast<pldm_state_sensor_pdr*>(pdr_second.data());

    auto state_second = reinterpret_cast<state_sensor_possible_states*>(
        rec_second->possible_states);

    rec_second->hdr.type = 4;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 66;
    rec_second->container_id = 0;
    rec_second->composite_sensor_count = 1;
    state_second->state_set_id = 3;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add_check(repo, pdr_second.data(), pdr_second.size(),
                                 false, 1, &handle),
              0);

    auto record = findStateSensorPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testOneMatchOneNoMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 5;
    uint16_t stateSetId = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_sensor_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_sensor_possible_states));

    auto rec = reinterpret_cast<pldm_state_sensor_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_sensor_possible_states*>(rec->possible_states);

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 10;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 20;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec_second =
        reinterpret_cast<pldm_state_sensor_pdr*>(pdr_second.data());

    auto state_second = reinterpret_cast<state_sensor_possible_states*>(
        rec_second->possible_states);

    rec_second->hdr.type = 4;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 5;
    rec_second->container_id = 0;
    rec_second->composite_sensor_count = 1;
    state_second->state_set_id = 1;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add_check(repo, pdr_second.data(), pdr_second.size(),
                                 false, 1, &handle),
              0);

    auto record = findStateSensorPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr_second, record[0]);
    EXPECT_EQ(record.size(), 1);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testOneMatchManyNoMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 5;
    uint16_t stateSetId = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_sensor_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_sensor_possible_states));

    auto rec = reinterpret_cast<pldm_state_sensor_pdr*>(pdr.data());

    auto state =
        reinterpret_cast<state_sensor_possible_states*>(rec->possible_states);

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 6;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 9;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec_second =
        reinterpret_cast<pldm_state_sensor_pdr*>(pdr_second.data());

    auto state_second = reinterpret_cast<state_sensor_possible_states*>(
        rec_second->possible_states);

    rec_second->hdr.type = 4;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 5;
    rec_second->container_id = 0;
    rec_second->composite_sensor_count = 1;
    state_second->state_set_id = 1;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add_check(repo, pdr_second.data(), pdr_second.size(),
                                 false, 1, &handle),
              0);

    std::vector<uint8_t> pdr_third(sizeof(struct pldm_state_sensor_pdr) -
                                   sizeof(uint8_t) +
                                   sizeof(struct state_sensor_possible_states));

    auto rec_third = reinterpret_cast<pldm_state_sensor_pdr*>(pdr_third.data());

    auto state_third = reinterpret_cast<state_sensor_possible_states*>(
        rec_third->possible_states);

    rec_third->hdr.type = 4;
    rec_third->hdr.record_handle = 3;
    rec_third->entity_type = 7;
    rec_third->container_id = 0;
    rec_third->composite_sensor_count = 1;
    state_third->state_set_id = 12;
    state_third->possible_states_size = 1;

    auto record = findStateSensorPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr_second, record[0]);
    EXPECT_EQ(record.size(), 1);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testCompositeSensor)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 5;
    uint16_t stateSetId = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_sensor_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_sensor_possible_states) * 3);

    auto rec = reinterpret_cast<pldm_state_sensor_pdr*>(pdr.data());
    auto state_start = rec->possible_states;

    auto state = reinterpret_cast<state_sensor_possible_states*>(state_start);

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 5;
    rec->container_id = 0;
    rec->composite_sensor_count = 3;
    state->state_set_id = 2;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = reinterpret_cast<state_sensor_possible_states*>(state_start);

    state->state_set_id = 7;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = reinterpret_cast<state_sensor_possible_states*>(state_start);

    state->state_set_id = 1;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    auto record = findStateSensorPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(pdr, record[0]);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testNoMatchCompositeSensor)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;
    uint16_t entityID = 5;
    uint16_t stateSetId = 1;

    std::vector<uint8_t> pdr(sizeof(struct pldm_state_sensor_pdr) -
                             sizeof(uint8_t) +
                             sizeof(struct state_sensor_possible_states) * 3);

    auto rec = reinterpret_cast<pldm_state_sensor_pdr*>(pdr.data());
    auto state_start = rec->possible_states;

    auto state = reinterpret_cast<state_sensor_possible_states*>(state_start);

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 21;
    rec->container_id = 0;
    rec->composite_sensor_count = 3;
    state->state_set_id = 15;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = reinterpret_cast<state_sensor_possible_states*>(state_start);
    state->state_set_id = 19;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = reinterpret_cast<state_sensor_possible_states*>(state_start);
    state->state_set_id = 39;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add_check(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    auto record = findStateSensorPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);

    pldm_pdr_destroy(repo);
}

TEST(toString, allTestCases)
{
    variable_field buffer{};
    constexpr std::string_view str1{};
    auto returnStr1 = toString(buffer);
    EXPECT_EQ(returnStr1, str1);

    constexpr std::string_view str2{"0penBmc"};
    constexpr std::array<uint8_t, 7> bufferArr1{0x30, 0x70, 0x65, 0x6e,
                                                0x42, 0x6d, 0x63};
    buffer.ptr = bufferArr1.data();
    buffer.length = bufferArr1.size();
    auto returnStr2 = toString(buffer);
    EXPECT_EQ(returnStr2, str2);

    constexpr std::string_view str3{"0pen mc"};
    // \xa0 - the non-breaking space in ISO-8859-1
    constexpr std::array<uint8_t, 7> bufferArr2{0x30, 0x70, 0x65, 0x6e,
                                                0xa0, 0x6d, 0x63};
    buffer.ptr = bufferArr2.data();
    buffer.length = bufferArr2.size();
    auto returnStr3 = toString(buffer);
    EXPECT_EQ(returnStr3, str3);
}

TEST(Split, allTestCases)
{
    std::string s1 = "aa,bb,cc,dd";
    auto results1 = split(s1, ",");
    EXPECT_EQ(results1[0], "aa");
    EXPECT_EQ(results1[1], "bb");
    EXPECT_EQ(results1[2], "cc");
    EXPECT_EQ(results1[3], "dd");

    std::string s2 = "aa||bb||cc||dd";
    auto results2 = split(s2, "||");
    EXPECT_EQ(results2[0], "aa");
    EXPECT_EQ(results2[1], "bb");
    EXPECT_EQ(results2[2], "cc");
    EXPECT_EQ(results2[3], "dd");

    std::string s3 = " aa || bb||cc|| dd";
    auto results3 = split(s3, "||", " ");
    EXPECT_EQ(results3[0], "aa");
    EXPECT_EQ(results3[1], "bb");
    EXPECT_EQ(results3[2], "cc");
    EXPECT_EQ(results3[3], "dd");

    std::string s4 = "aa\\\\bb\\cc\\dd";
    auto results4 = split(s4, "\\");
    EXPECT_EQ(results4[0], "aa");
    EXPECT_EQ(results4[1], "bb");
    EXPECT_EQ(results4[2], "cc");
    EXPECT_EQ(results4[3], "dd");

    std::string s5 = "aa\\";
    auto results5 = split(s5, "\\");
    EXPECT_EQ(results5[0], "aa");
}
