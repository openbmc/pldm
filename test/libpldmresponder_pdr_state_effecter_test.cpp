#include "libpldmresponder/effecters.hpp"
#include "libpldmresponder/pdr.hpp"

#include "libpldm/platform.h"

#include <gtest/gtest.h>

using namespace pldm::responder;
using namespace pdr::internal;

TEST(GeneratePDR, testGoodJson)
{
    using namespace effecter::dbus_mapping;

    IndexedRepo pdrRepo;
    getRepoByType("./pdr_jsons/state_effecter/good", PLDM_STATE_EFFECTER_PDR,
                  pdrRepo);
    // 2 entries
    ASSERT_EQ(pdrRepo.numEntries(), 2);

    // Check first PDR
    pdr::Entry e = pdrRepo.at(1);
    pldm_state_effecter_pdr* pdr =
        reinterpret_cast<pldm_state_effecter_pdr*>(e.data());

    ASSERT_EQ(pdr->hdr.record_handle, 1);
    ASSERT_EQ(pdr->hdr.version, 1);
    ASSERT_EQ(pdr->hdr.type, PLDM_STATE_EFFECTER_PDR);
    ASSERT_EQ(pdr->hdr.record_change_num, 0);
    ASSERT_EQ(pdr->hdr.length, 23);

    ASSERT_EQ(pdr->terminus_handle, 0);
    ASSERT_EQ(pdr->effecter_id, 1);
    ASSERT_EQ(pdr->entity_type, 33);
    ASSERT_EQ(pdr->entity_instance, 0);
    ASSERT_EQ(pdr->container_id, 0);
    ASSERT_EQ(pdr->effecter_semantic_id, 0);
    ASSERT_EQ(pdr->effecter_init, PLDM_NO_INIT);
    ASSERT_EQ(pdr->has_description_pdr, false);
    ASSERT_EQ(pdr->composite_effecter_count, 2);
    state_effecter_possible_states* states =
        reinterpret_cast<state_effecter_possible_states*>(pdr->possible_states);
    ASSERT_EQ(states->state_set_id, 196);
    ASSERT_EQ(states->possible_states_size, 1);
    bitfield8_t bf1{};
    bf1.byte = 2;
    ASSERT_EQ(states->states[0].byte, bf1.byte);

    auto dbusObj = get(pdr->effecter_id);
    ASSERT_EQ(dbusObj[0].objectPath, "/foo/bar");

    // Check second PDR
    e = pdrRepo.at(2);
    pdr = reinterpret_cast<pldm_state_effecter_pdr*>(e.data());

    ASSERT_EQ(pdr->hdr.record_handle, 2);
    ASSERT_EQ(pdr->hdr.version, 1);
    ASSERT_EQ(pdr->hdr.type, PLDM_STATE_EFFECTER_PDR);
    ASSERT_EQ(pdr->hdr.record_change_num, 0);
    ASSERT_EQ(pdr->hdr.length, 24);

    ASSERT_EQ(pdr->terminus_handle, 0);
    ASSERT_EQ(pdr->effecter_id, 2);
    ASSERT_EQ(pdr->entity_type, 100);
    ASSERT_EQ(pdr->entity_instance, 0);
    ASSERT_EQ(pdr->container_id, 0);
    ASSERT_EQ(pdr->effecter_semantic_id, 0);
    ASSERT_EQ(pdr->effecter_init, PLDM_NO_INIT);
    ASSERT_EQ(pdr->has_description_pdr, false);
    ASSERT_EQ(pdr->composite_effecter_count, 2);
    states =
        reinterpret_cast<state_effecter_possible_states*>(pdr->possible_states);
    ASSERT_EQ(states->state_set_id, 197);
    ASSERT_EQ(states->possible_states_size, 1);
    bf1.byte = 2;
    ASSERT_EQ(states->states[0].byte, bf1.byte);
    states = reinterpret_cast<state_effecter_possible_states*>(
        pdr->possible_states + sizeof(state_effecter_possible_states));
    ASSERT_EQ(states->state_set_id, 198);
    ASSERT_EQ(states->possible_states_size, 2);
    bitfield8_t bf2[2];
    bf2[0].byte = 38;
    bf2[1].byte = 128;
    ASSERT_EQ(states->states[0].byte, bf2[0].byte);
    ASSERT_EQ(states->states[1].byte, bf2[1].byte);

    dbusObj = get(pdr->effecter_id);
    ASSERT_EQ(dbusObj[0].objectPath, "/foo/bar");
    ASSERT_EQ(dbusObj[1].objectPath, "/foo/bar/baz");

    ASSERT_THROW(get(0xDEAD), std::exception);
}

TEST(GeneratePDR, testNoJson)
{
    IndexedRepo pdrRepo;
    ASSERT_THROW(getRepoByType("./pdr_jsons/not_there", PLDM_STATE_EFFECTER_PDR,
                               pdrRepo),
                 std::exception);
}

TEST(GeneratePDR, testMalformedJson)
{
    IndexedRepo pdrRepo;
    getRepoByType("./pdr_jsons/state_effecter/good", PLDM_STATE_EFFECTER_PDR,
                  pdrRepo);
    ASSERT_EQ(pdrRepo.numEntries(), 2);
    pdrRepo.makeEmpty();
    ASSERT_THROW(pldm::responder::pdr::internal::readJson(
                     "./pdr_jsons/state_effecter/malformed"),
                 std::exception);
}
