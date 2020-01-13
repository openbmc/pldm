#include "libpldmresponder/effecters.hpp"
#include "libpldmresponder/pdr.hpp"

#include "libpldm/platform.h"

#include <gtest/gtest.h>

using namespace pldm::responder;
using namespace pdr::internal;

TEST(GeneratePDR, testGoodJson)
{
    using namespace pdr;
    using namespace effecter::dbus_mapping;

    IndexedRepo pdrRepo;
    getRepoByType("./pdr_jsons/state_effecter/good", PLDM_NUMERIC_EFFECTER_PDR,
                  pdrRepo);
    // 1 entries
    ASSERT_EQ(pdrRepo.numEntries(), 1);

    // Check first PDR
    pdr::Entry e = pdrRepo.at(1);
    pldm_numeric_effecter_value_pdr* pdr =
        reinterpret_cast<pldm_numeric_effecter_value_pdr*>(e.data());
    EXPECT_EQ(pdr->hdr.record_handle, 1);
    EXPECT_EQ(pdr->hdr.version, 1);
    EXPECT_EQ(pdr->hdr.type, PLDM_NUMERIC_EFFECTER_PDR);
    EXPECT_EQ(pdr->hdr.record_change_num, 0);
    EXPECT_EQ(pdr->hdr.length,
              sizeof(pldm_numeric_effecter_value_pdr) - sizeof(pldm_pdr_hdr));

    EXPECT_EQ(pdr->effecter_id, 32768);
    EXPECT_EQ(pdr->effecter_data_size, 4);

    auto dbusObj = get(pdr->effecter_id);
    EXPECT_EQ(dbusObj[0].objectPath, "/xyz/openbmc_project/foo/bar");
    EXPECT_EQ(dbusObj[0].interface, "xyz.openbmc_project.foo.bar");
    EXPECT_EQ(dbusObj[0].propertyName, "property");
}

TEST(GeneratePDR, testNoJson)
{
    IndexedRepo pdrRepo;
    ASSERT_THROW(getRepoByType("./pdr_jsons/not_there",
                               PLDM_NUMERIC_EFFECTER_PDR, pdrRepo),
                 std::exception);
}

TEST(GeneratePDR, testMalformedJson)
{
    IndexedRepo pdrRepo;
    getRepoByType("./pdr_jsons/state_effecter/good", PLDM_NUMERIC_EFFECTER_PDR,
                  pdrRepo);
    ASSERT_EQ(pdrRepo.numEntries(), 1);
    pdrRepo.makeEmpty();
    ASSERT_THROW(
        readJson(
            "./pdr_jsons/state_effecter/malformed/numeric_effecter_pdr.json"),
        std::exception);
}
