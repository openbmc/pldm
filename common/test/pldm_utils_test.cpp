#include "common/utils.hpp"
#include "mocked_utils.hpp"

#include <libpldm/platform.h>
#include <linux/mctp.h>

#include <gtest/gtest.h>

using namespace pldm::utils;

TEST(GetNumPadBytesTest, NoPaddingNeeded)
{
    EXPECT_EQ(getNumPadBytes(0), 0);
    EXPECT_EQ(getNumPadBytes(4), 0);
    EXPECT_EQ(getNumPadBytes(8), 0);
    EXPECT_EQ(getNumPadBytes(12), 0);
}

TEST(GetNumPadBytesTest, OneBytePadding)
{
    EXPECT_EQ(getNumPadBytes(3), 1);
    EXPECT_EQ(getNumPadBytes(7), 1);
    EXPECT_EQ(getNumPadBytes(11), 1);
}

TEST(GetNumPadBytesTest, TwoBytesPadding)
{
    EXPECT_EQ(getNumPadBytes(2), 2);
    EXPECT_EQ(getNumPadBytes(6), 2);
    EXPECT_EQ(getNumPadBytes(10), 2);
}

TEST(GetNumPadBytesTest, ThreeBytesPadding)
{
    EXPECT_EQ(getNumPadBytes(1), 3);
    EXPECT_EQ(getNumPadBytes(5), 3);
    EXPECT_EQ(getNumPadBytes(9), 3);
}

TEST(GetNumPadBytesTest, LargeValues)
{
    EXPECT_EQ(getNumPadBytes(1001), 3);
    EXPECT_EQ(getNumPadBytes(1024), 0);
    EXPECT_EQ(getNumPadBytes(65535), 1);
}

TEST(GetInventoryObjects, testForEmptyObject)
{
    ObjectValueTree result =
        DBusHandler::getInventoryObjects<GetManagedEmptyObject>();
    EXPECT_TRUE(result.empty());
}

TEST(GetInventoryObjects, testForObject)
{
    std::string path = "/foo/bar";
    std::string service = "foo.bar";
    auto result = DBusHandler::getInventoryObjects<GetManagedObject>();
    EXPECT_EQ(result[path].begin()->first, service);
    auto function =
        std::get<bool>(result[path][service][std::string("Functional")]);
    auto model =
        std::get<std::string>(result[path][service][std::string("Model")]);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(function);
    EXPECT_EQ(model, std::string("1234 - 00Z"));
}

TEST(printBuffer, testprintBufferGoodPath)
{
    std::vector<uint8_t> buffer = {10, 12, 14, 25, 233};
    std::ostringstream localString;
    auto coutBuffer = std::cout.rdbuf();
    std::cout.rdbuf(localString.rdbuf());
    printBuffer(false, buffer);
    std::cout.rdbuf(coutBuffer);
    EXPECT_EQ(localString.str(), "Rx: 0a 0c 0e 19 e9 \n");
    localString.str("");
    localString.clear();
    std::cerr << localString.str() << std::endl;
    buffer = {12, 0, 200, 12, 255};
    std::cout.rdbuf(localString.rdbuf());
    printBuffer(true, buffer);
    std::cout.rdbuf(coutBuffer);
    EXPECT_EQ(localString.str(), "Tx: 0c 00 c8 0c ff \n");
}

TEST(printBuffer, testprintBufferBadPath)
{
    std::vector<uint8_t> buffer = {};
    std::ostringstream localString;
    auto coutBuffer = std::cout.rdbuf();
    std::cout.rdbuf(localString.rdbuf());
    printBuffer(false, buffer);
    EXPECT_EQ(localString.str(), "");
    printBuffer(true, buffer);
    std::cout.rdbuf(coutBuffer);
    EXPECT_EQ(localString.str(), "");
}

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec = new (pdr.data()) pldm_state_effecter_pdr;

    auto state = new (rec->possible_states) state_effecter_possible_states;

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 33;
    rec->container_id = 0;
    rec->composite_effecter_count = 1;
    state->state_set_id = 196;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec = new (pdr.data()) pldm_state_effecter_pdr;

    auto state = new (rec->possible_states) state_effecter_possible_states;

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 33;
    rec->container_id = 0;
    rec->composite_effecter_count = 1;
    state->state_set_id = 196;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto record = findStateEffecterPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);

    pldm_pdr_destroy(repo);
}

TEST(FindStateEffecterPDR, testMoreMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec = new (pdr.data()) pldm_state_effecter_pdr;

    auto state = new (rec->possible_states) state_effecter_possible_states;

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 31;
    rec->container_id = 0;
    rec->composite_effecter_count = 1;
    state->state_set_id = 129;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_second = new (pdr_second.data()) pldm_state_effecter_pdr;

    auto state_second = new (rec_second->possible_states)
        state_effecter_possible_states;

    rec_second->hdr.type = 11;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 31;
    rec_second->container_id = 0;
    rec_second->composite_effecter_count = 1;
    state_second->state_set_id = 129;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), false, 1,
                           &handle),
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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec = new (pdr.data()) pldm_state_effecter_pdr;

    auto state = new (rec->possible_states) state_effecter_possible_states;

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 34;
    rec->container_id = 0;
    rec->composite_effecter_count = 1;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_second = new (pdr_second.data()) pldm_state_effecter_pdr;

    auto state_second = new (rec_second->possible_states)
        state_effecter_possible_states;

    rec_second->hdr.type = 11;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 39;
    rec_second->container_id = 0;
    rec_second->composite_effecter_count = 1;
    state_second->state_set_id = 169;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), false, 1,
                           &handle),
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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec = new (pdr.data()) pldm_state_effecter_pdr;

    auto state = new (rec->possible_states) state_effecter_possible_states;

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 32;
    rec->container_id = 0;
    rec->composite_effecter_count = 1;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_second = new (pdr_second.data()) pldm_state_effecter_pdr;

    auto state_second = new (rec_second->possible_states)
        state_effecter_possible_states;

    rec_second->hdr.type = 11;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 67;
    rec_second->container_id = 0;
    rec_second->composite_effecter_count = 1;
    state_second->state_set_id = 192;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), false, 1,
                           &handle),
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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec = new (pdr.data()) pldm_state_effecter_pdr;

    auto state = new (rec->possible_states) state_effecter_possible_states;

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 32;
    rec->container_id = 0;
    rec->composite_effecter_count = 1;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_second = new (pdr_second.data()) pldm_state_effecter_pdr;

    auto state_second = new (rec_second->possible_states)
        state_effecter_possible_states;

    rec_second->hdr.type = 11;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 67;
    rec_second->container_id = 0;
    rec_second->composite_effecter_count = 1;
    state_second->state_set_id = 192;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), false, 1,
                           &handle),
              0);

    std::vector<uint8_t> pdr_third(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states));

    auto rec_third = new (pdr_third.data()) pldm_state_effecter_pdr;

    auto state_third = new (rec_third->possible_states)
        state_effecter_possible_states;

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states) * 3);

    auto rec = new (pdr.data()) pldm_state_effecter_pdr;
    auto state_start = rec->possible_states;

    auto state = new (state_start) state_effecter_possible_states;

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 67;
    rec->container_id = 0;
    rec->composite_effecter_count = 3;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = new (state_start) state_effecter_possible_states;
    state->state_set_id = 193;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = new (state_start) state_effecter_possible_states;
    state->state_set_id = 192;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(struct state_effecter_possible_states) * 3);

    auto rec = new (pdr.data()) pldm_state_effecter_pdr;
    auto state_start = rec->possible_states;

    auto state = new (state_start) state_effecter_possible_states;

    rec->hdr.type = 11;
    rec->hdr.record_handle = 1;
    rec->entity_type = 34;
    rec->container_id = 0;
    rec->composite_effecter_count = 3;
    state->state_set_id = 198;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = new (state_start) state_effecter_possible_states;
    state->state_set_id = 193;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = new (state_start) state_effecter_possible_states;
    state->state_set_id = 123;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec = new (pdr.data()) pldm_state_sensor_pdr;

    auto state = new (rec->possible_states) state_sensor_possible_states;

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 5;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 1;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec = new (pdr.data()) pldm_state_sensor_pdr;

    auto state = new (rec->possible_states) state_sensor_possible_states;

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 55;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 1;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto record = findStateSensorPDR(tid, entityID, stateSetId, repo);

    EXPECT_EQ(record.empty(), true);

    pldm_pdr_destroy(repo);
}

TEST(FindStateSensorPDR, testMoreMatch)
{
    auto repo = pldm_pdr_init();
    uint8_t tid = 1;

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec = new (pdr.data()) pldm_state_sensor_pdr;

    auto state = new (rec->possible_states) state_sensor_possible_states;

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 5;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 1;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec_second = new (pdr_second.data()) pldm_state_sensor_pdr;

    auto state_second = new (rec_second->possible_states)
        state_sensor_possible_states;

    rec_second->hdr.type = 4;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 5;
    rec_second->container_id = 0;
    rec_second->composite_sensor_count = 1;
    state_second->state_set_id = 1;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), false, 1,
                           &handle),
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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec = new (pdr.data()) pldm_state_sensor_pdr;

    auto state = new (rec->possible_states) state_sensor_possible_states;

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 56;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 2;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec_second = new (pdr_second.data()) pldm_state_sensor_pdr;

    auto state_second = new (rec_second->possible_states)
        state_sensor_possible_states;

    rec_second->hdr.type = 4;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 66;
    rec_second->container_id = 0;
    rec_second->composite_sensor_count = 1;
    state_second->state_set_id = 3;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), false, 1,
                           &handle),
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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec = new (pdr.data()) pldm_state_sensor_pdr;

    auto state = new (rec->possible_states) state_sensor_possible_states;

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 10;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 20;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec_second = new (pdr_second.data()) pldm_state_sensor_pdr;

    auto state_second = new (rec_second->possible_states)
        state_sensor_possible_states;

    rec_second->hdr.type = 4;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 5;
    rec_second->container_id = 0;
    rec_second->composite_sensor_count = 1;
    state_second->state_set_id = 1;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), false, 1,
                           &handle),
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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec = new (pdr.data()) pldm_state_sensor_pdr;

    auto state = new (rec->possible_states) state_sensor_possible_states;

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 6;
    rec->container_id = 0;
    rec->composite_sensor_count = 1;
    state->state_set_id = 9;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    std::vector<uint8_t> pdr_second(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec_second = new (pdr_second.data()) pldm_state_sensor_pdr;

    auto state_second = new (rec_second->possible_states)
        state_sensor_possible_states;

    rec_second->hdr.type = 4;
    rec_second->hdr.record_handle = 2;
    rec_second->entity_type = 5;
    rec_second->container_id = 0;
    rec_second->composite_sensor_count = 1;
    state_second->state_set_id = 1;
    state_second->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr_second.data(), pdr_second.size(), false, 1,
                           &handle),
              0);

    std::vector<uint8_t> pdr_third(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states));

    auto rec_third = new (pdr_third.data()) pldm_state_sensor_pdr;

    auto state_third = new (rec_third->possible_states)
        state_sensor_possible_states;

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states) * 3);

    auto rec = new (pdr.data()) pldm_state_sensor_pdr;
    auto state_start = rec->possible_states;

    auto state = new (state_start) state_sensor_possible_states;

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 5;
    rec->container_id = 0;
    rec->composite_sensor_count = 3;
    state->state_set_id = 2;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = new (state_start) state_sensor_possible_states;

    state->state_set_id = 7;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = new (state_start) state_sensor_possible_states;

    state->state_set_id = 1;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

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

    std::vector<uint8_t> pdr(
        sizeof(struct pldm_state_sensor_pdr) - sizeof(uint8_t) +
        sizeof(struct state_sensor_possible_states) * 3);

    auto rec = new (pdr.data()) pldm_state_sensor_pdr;
    auto state_start = rec->possible_states;

    auto state = new (state_start) state_sensor_possible_states;

    rec->hdr.type = 4;
    rec->hdr.record_handle = 1;
    rec->entity_type = 21;
    rec->container_id = 0;
    rec->composite_sensor_count = 3;
    state->state_set_id = 15;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = new (state_start) state_sensor_possible_states;
    state->state_set_id = 19;
    state->possible_states_size = 1;

    state_start += state->possible_states_size + sizeof(state->state_set_id) +
                   sizeof(state->possible_states_size);
    state = new (state_start) state_sensor_possible_states;
    state->state_set_id = 39;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

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

TEST(ValidEID, allTestCases)
{
    auto rc = isValidEID(MCTP_ADDR_NULL);
    EXPECT_EQ(rc, false);
    rc = isValidEID(MCTP_ADDR_ANY);
    EXPECT_EQ(rc, false);
    rc = isValidEID(1);
    EXPECT_EQ(rc, false);
    rc = isValidEID(2);
    EXPECT_EQ(rc, false);
    rc = isValidEID(3);
    EXPECT_EQ(rc, false);
    rc = isValidEID(4);
    EXPECT_EQ(rc, false);
    rc = isValidEID(5);
    EXPECT_EQ(rc, false);
    rc = isValidEID(6);
    EXPECT_EQ(rc, false);
    rc = isValidEID(7);
    EXPECT_EQ(rc, false);
    rc = isValidEID(MCTP_START_VALID_EID);
    EXPECT_EQ(rc, true);
    rc = isValidEID(254);
    EXPECT_EQ(rc, true);
}

TEST(TrimNameForDbus, goodTest)
{
    std::string name = "Name with  space";
    std::string_view expectedName = "Name_with__space";
    std::string_view result = trimNameForDbus(name);
    EXPECT_EQ(expectedName, result);
    name = "Name 1\0"; // NOLINT(bugprone-string-literal-with-embedded-nul)
    expectedName = "Name_1";
    result = trimNameForDbus(name);
    EXPECT_EQ(expectedName, result);
}

TEST(dbusPropValuesToDouble, goodTest)
{
    double value = 0;
    bool ret =
        dbusPropValuesToDouble("uint8_t", static_cast<uint8_t>(0x12), &value);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(0x12, value);
    ret =
        dbusPropValuesToDouble("int16_t", static_cast<int16_t>(0x1234), &value);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(0x1234, value);
    ret = dbusPropValuesToDouble("uint16_t", static_cast<uint16_t>(0x8234),
                                 &value);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(0x8234, value);
    ret = dbusPropValuesToDouble("int32_t", static_cast<int32_t>(0x12345678),
                                 &value);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(0x12345678, value);
    ret = dbusPropValuesToDouble("uint32_t", static_cast<uint32_t>(0x82345678),
                                 &value);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(0x82345678, value);
    ret = dbusPropValuesToDouble(
        "int64_t", static_cast<int64_t>(0x1234567898765432), &value);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(0x1234567898765432, value);
    ret = dbusPropValuesToDouble(
        "uint64_t", static_cast<uint64_t>(0x8234567898765432), &value);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(0x8234567898765432, value);
    ret = dbusPropValuesToDouble("double", static_cast<double>(1234.5678),
                                 &value);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(1234.5678, value);
}

TEST(dbusPropValuesToDouble, badTest)
{
    double value = std::numeric_limits<double>::quiet_NaN();
    /* Type and Data variant are different */
    bool ret =
        dbusPropValuesToDouble("uint8_t", static_cast<uint16_t>(0x12), &value);
    EXPECT_EQ(false, ret);
    /* Unsupported Types */
    ret = dbusPropValuesToDouble("string", static_cast<std::string>("hello"),
                                 &value);
    EXPECT_EQ(false, ret);
    ret = dbusPropValuesToDouble("bool", static_cast<bool>(true), &value);
    EXPECT_EQ(false, ret);
    ret = dbusPropValuesToDouble("vector<uint8_t>",
                                 static_cast<std::string>("hello"), &value);
    EXPECT_EQ(false, ret);
    ret = dbusPropValuesToDouble("vector<string>",
                                 static_cast<std::string>("hello"), &value);
    EXPECT_EQ(false, ret);
    /* Support Type but Data Type is unsupported */
    ret = dbusPropValuesToDouble("double", static_cast<std::string>("hello"),
                                 &value);
    EXPECT_EQ(false, ret);
    /* Null pointer */
    ret = dbusPropValuesToDouble("double", static_cast<std::string>("hello"),
                                 nullptr);
    EXPECT_EQ(false, ret);
}

TEST(FruFieldValuestring, goodTest)
{
    std::vector<uint8_t> data = {0x41, 0x6d, 0x70, 0x65, 0x72, 0x65};
    std::string expectedString = "Ampere";
    auto result = fruFieldValuestring(data.data(), data.size());
    EXPECT_EQ(expectedString, result);
}

TEST(FruFieldValuestring, BadTest)
{
    std::vector<uint8_t> data = {0x41, 0x6d, 0x70, 0x65, 0x72, 0x65};
    auto result = fruFieldValuestring(data.data(), 0);
    EXPECT_EQ(std::nullopt, result);
    result = fruFieldValuestring(nullptr, data.size());
    EXPECT_EQ(std::nullopt, result);
}

TEST(fruFieldParserU32, goodTest)
{
    std::vector<uint8_t> data = {0x10, 0x12, 0x14, 0x25};
    uint32_t expectedU32 = 0x25141210;
    auto result = fruFieldParserU32(data.data(), data.size());
    EXPECT_EQ(expectedU32, result.value());
}

TEST(fruFieldParserU32, BadTest)
{
    std::vector<uint8_t> data = {0x10, 0x12, 0x14, 0x25};
    auto result = fruFieldParserU32(data.data(), data.size() - 1);
    EXPECT_EQ(std::nullopt, result);
    result = fruFieldParserU32(nullptr, data.size());
    EXPECT_EQ(std::nullopt, result);
}
TEST(CheckIfLogicalBitSet, allTestCases)
{
    // Test when logical bit is set (bit 15 is 0)
    EXPECT_TRUE(checkIfLogicalBitSet(0x0000));
    EXPECT_TRUE(checkIfLogicalBitSet(0x0001));
    EXPECT_TRUE(checkIfLogicalBitSet(0x7FFF));
    EXPECT_TRUE(checkIfLogicalBitSet(0x1234));

    // Test when logical bit is not set (bit 15 is 1)
    EXPECT_FALSE(checkIfLogicalBitSet(0x8000));
    EXPECT_FALSE(checkIfLogicalBitSet(0x8001));
    EXPECT_FALSE(checkIfLogicalBitSet(0xFFFF));
    EXPECT_FALSE(checkIfLogicalBitSet(0xABCD));
}

TEST(DecimalToBcd, allTestCases)
{
    // Single digit conversions
    EXPECT_EQ(decimalToBcd(0), 0x00);
    EXPECT_EQ(decimalToBcd(5), 0x05);
    EXPECT_EQ(decimalToBcd(9), 0x09);

    // Two digit conversions
    EXPECT_EQ(decimalToBcd(10), 0x10);
    EXPECT_EQ(decimalToBcd(25), 0x25);
    EXPECT_EQ(decimalToBcd(99), 0x99);

    // Multi-digit conversions
    EXPECT_EQ(decimalToBcd(123), 0x123);
    EXPECT_EQ(decimalToBcd(456), 0x456);
    EXPECT_EQ(decimalToBcd(9999), 0x9999);

    // Test with different integer types
    EXPECT_EQ(decimalToBcd<uint8_t>(42), 0x42);
    EXPECT_EQ(decimalToBcd<uint16_t>(1234), 0x1234);
    EXPECT_EQ(decimalToBcd<uint32_t>(56789), 0x56789);
}

TEST(JsonEntryToDbusVal, uint8Test)
{
    nlohmann::json value = 42;
    auto result = jsonEntryToDbusVal("uint8_t", value);
    EXPECT_EQ(std::get<uint8_t>(result), 42);
}

TEST(JsonEntryToDbusVal, uint16Test)
{
    nlohmann::json value = 1234;
    auto result = jsonEntryToDbusVal("uint16_t", value);
    EXPECT_EQ(std::get<uint16_t>(result), 1234);
}

TEST(JsonEntryToDbusVal, uint32Test)
{
    nlohmann::json value = 123456;
    auto result = jsonEntryToDbusVal("uint32_t", value);
    EXPECT_EQ(std::get<uint32_t>(result), 123456);
}

TEST(JsonEntryToDbusVal, uint64Test)
{
    nlohmann::json value = 9876543210;
    auto result = jsonEntryToDbusVal("uint64_t", value);
    EXPECT_EQ(std::get<uint64_t>(result), 9876543210);
}

TEST(JsonEntryToDbusVal, int16Test)
{
    nlohmann::json value = -1234;
    auto result = jsonEntryToDbusVal("int16_t", value);
    EXPECT_EQ(std::get<int16_t>(result), -1234);
}

TEST(JsonEntryToDbusVal, int32Test)
{
    nlohmann::json value = -123456;
    auto result = jsonEntryToDbusVal("int32_t", value);
    EXPECT_EQ(std::get<int32_t>(result), -123456);
}

TEST(JsonEntryToDbusVal, int64Test)
{
    nlohmann::json value = -9876543210;
    auto result = jsonEntryToDbusVal("int64_t", value);
    EXPECT_EQ(std::get<int64_t>(result), -9876543210);
}

TEST(JsonEntryToDbusVal, boolTest)
{
    nlohmann::json valueTrue = true;
    auto resultTrue = jsonEntryToDbusVal("bool", valueTrue);
    EXPECT_EQ(std::get<bool>(resultTrue), true);

    nlohmann::json valueFalse = false;
    auto resultFalse = jsonEntryToDbusVal("bool", valueFalse);
    EXPECT_EQ(std::get<bool>(resultFalse), false);
}

TEST(JsonEntryToDbusVal, doubleTest)
{
    nlohmann::json value = 123.456;
    auto result = jsonEntryToDbusVal("double", value);
    EXPECT_DOUBLE_EQ(std::get<double>(result), 123.456);
}

TEST(JsonEntryToDbusVal, stringTest)
{
    nlohmann::json value = "test_string";
    auto result = jsonEntryToDbusVal("string", value);
    EXPECT_EQ(std::get<std::string>(result), "test_string");
}

TEST(GetStateSensorPDRsByType, testMultipleMatches)
{
    auto repo = pldm_pdr_init();
    uint16_t entityType = 5;

    // Add first PDR
    std::vector<uint8_t> pdr1(sizeof(pldm_state_sensor_pdr) - sizeof(uint8_t) +
                              sizeof(state_sensor_possible_states));

    auto* rec1 = new (pdr1.data()) pldm_state_sensor_pdr{};
    auto* state1 = new (rec1->possible_states) state_sensor_possible_states{};

    rec1->hdr.type = PLDM_STATE_SENSOR_PDR;
    rec1->hdr.record_handle = 1;
    rec1->entity_type = entityType;
    rec1->composite_sensor_count = 1;
    state1->state_set_id = 1;
    state1->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr1.data(), pdr1.size(), false, 1, &handle),
              0);

    // Add second PDR
    std::vector<uint8_t> pdr2(sizeof(pldm_state_sensor_pdr) - sizeof(uint8_t) +
                              sizeof(state_sensor_possible_states));

    auto* rec2 = new (pdr2.data()) pldm_state_sensor_pdr{};
    auto* state2 = new (rec2->possible_states) state_sensor_possible_states{};

    rec2->hdr.type = PLDM_STATE_SENSOR_PDR;
    rec2->hdr.record_handle = 2;
    rec2->entity_type = entityType;
    rec2->composite_sensor_count = 1;
    state2->state_set_id = 2;
    state2->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr2.data(), pdr2.size(), false, 1, &handle),
              0);

    auto pdrs = getStateSensorPDRsByType(entityType, repo);
    EXPECT_EQ(pdrs.size(), 2);

    pldm_pdr_destroy(repo);
}

TEST(GetStateSensorPDRsByType, testNoMatch)
{
    auto repo = pldm_pdr_init();
    uint16_t entityType = 5;

    std::vector<uint8_t> pdr(sizeof(pldm_state_sensor_pdr) - sizeof(uint8_t) +
                             sizeof(state_sensor_possible_states));

    auto* rec = new (pdr.data()) pldm_state_sensor_pdr{};
    auto* state = new (rec->possible_states) state_sensor_possible_states{};

    rec->hdr.type = PLDM_STATE_SENSOR_PDR;
    rec->hdr.record_handle = 1;
    rec->entity_type = 99;
    rec->composite_sensor_count = 1;
    state->state_set_id = 1;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 1, &handle), 0);

    auto pdrs = getStateSensorPDRsByType(entityType, repo);
    EXPECT_TRUE(pdrs.empty());

    pldm_pdr_destroy(repo);
}

TEST(GetStateSensorPDRsByType, testEmptyRepo)
{
    auto repo = pldm_pdr_init();

    auto pdrs = getStateSensorPDRsByType(5, repo);
    EXPECT_TRUE(pdrs.empty());

    pldm_pdr_destroy(repo);
}

TEST(GetStateEffecterPDRsByType, testMultipleMatches)
{
    auto repo = pldm_pdr_init();
    uint16_t entityType = 33;

    // Add first PDR
    std::vector<uint8_t> pdr1(
        sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(state_effecter_possible_states));

    auto* rec1 = new (pdr1.data()) pldm_state_effecter_pdr{};
    auto* state1 = new (rec1->possible_states) state_effecter_possible_states{};

    rec1->hdr.type = PLDM_STATE_EFFECTER_PDR;
    rec1->hdr.record_handle = 1;
    rec1->entity_type = entityType;
    rec1->composite_effecter_count = 1;
    state1->state_set_id = 196;
    state1->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(
        pldm_pdr_add(repo, pdr1.data(), pdr1.size(), false, 0xFF, &handle), 0);

    // Add the second PDR
    std::vector<uint8_t> pdr2(
        sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t) +
        sizeof(state_effecter_possible_states));

    auto* rec2 = new (pdr2.data()) pldm_state_effecter_pdr{};
    auto* state2 = new (rec2->possible_states) state_effecter_possible_states{};

    rec2->hdr.type = PLDM_STATE_EFFECTER_PDR;
    rec2->hdr.record_handle = 2;
    rec2->entity_type = entityType;
    rec2->composite_effecter_count = 1;
    state2->state_set_id = 197;
    state2->possible_states_size = 1;

    handle = 0;
    ASSERT_EQ(
        pldm_pdr_add(repo, pdr2.data(), pdr2.size(), false, 0xFF, &handle), 0);

    auto pdrs = getStateEffecterPDRsByType(entityType, repo);
    EXPECT_EQ(pdrs.size(), 2);

    pldm_pdr_destroy(repo);
}

TEST(GetStateEffecterPDRsByType, testNoMatch)
{
    auto repo = pldm_pdr_init();
    uint16_t entityType = 33;

    std::vector<uint8_t> pdr(sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t) +
                             sizeof(state_effecter_possible_states));

    auto* rec = new (pdr.data()) pldm_state_effecter_pdr{};
    auto* state = new (rec->possible_states) state_effecter_possible_states{};

    rec->hdr.type = PLDM_STATE_EFFECTER_PDR;
    rec->hdr.record_handle = 1;
    rec->entity_type = 99;
    rec->composite_effecter_count = 1;
    state->state_set_id = 196;
    state->possible_states_size = 1;

    uint32_t handle = 0;
    ASSERT_EQ(pldm_pdr_add(repo, pdr.data(), pdr.size(), false, 0xFF, &handle),
              0);

    auto pdrs = getStateEffecterPDRsByType(entityType, repo);
    EXPECT_TRUE(pdrs.empty());

    pldm_pdr_destroy(repo);
}

TEST(GetStateEffecterPDRsByType, testEmptyRepo)
{
    auto repo = pldm_pdr_init();

    auto pdrs = getStateEffecterPDRsByType(33, repo);

    EXPECT_TRUE(pdrs.empty());

    pldm_pdr_destroy(repo);
}

TEST(GenerateSwId, testRandomness)
{
    // Test that generateSwId returns values in valid range
    for (int i = 0; i < 100; ++i)
    {
        auto id = generateSwId();
        EXPECT_GE(id, 0);
        EXPECT_LT(id, 10000);
    }

    // Test that multiple calls can produce different values
    std::set<long int> ids;
    for (int i = 0; i < 50; ++i)
    {
        ids.insert(generateSwId());
    }
    // With 50 calls, we should get at least some different values
    EXPECT_GT(ids.size(), 1);
}
