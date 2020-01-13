#include "libpldmresponder/effecters.hpp"
#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/pdr_utils.hpp"

#include "libpldm/platform.h"

#include <gtest/gtest.h>

using namespace pldm::responder;

TEST(GeneratePDR, testGoodJson)
{
    using namespace effecter::dbus_mapping;
    pdr_utils::Repo pdrRepo = pdr::getRepoByType(
        "./pdr_jsons/state_effecter/good", PLDM_NUMERIC_EFFECTER_PDR);

    // 1 entries
    ASSERT_EQ(pdrRepo.getRecordCount(), 1);

    // Check first PDR
    pdr_utils::PdrEntry e;
    auto record = pdr::getRecordByHandle(pdrRepo, 3, e);
    ASSERT_NE(record, nullptr);

    pldm_numeric_effecter_value_pdr* pdr =
        reinterpret_cast<pldm_numeric_effecter_value_pdr*>(e.data);
    EXPECT_EQ(pdr->hdr.record_handle, 3);
    EXPECT_EQ(pdr->hdr.version, 1);
    EXPECT_EQ(pdr->hdr.type, PLDM_NUMERIC_EFFECTER_PDR);
    EXPECT_EQ(pdr->hdr.record_change_num, 0);
    EXPECT_EQ(pdr->hdr.length,
              sizeof(pldm_numeric_effecter_value_pdr) - sizeof(pldm_pdr_hdr));

    EXPECT_EQ(pdr->effecter_id, 3);
    EXPECT_EQ(pdr->effecter_data_size, 4);

    const auto& dbusObjs = getDbusObjs(pdr->effecter_id);
    EXPECT_EQ(dbusObjs[0].objectPath, "/foo/bar");
    EXPECT_EQ(dbusObjs[0].interface, "xyz.openbmc_project.Foo.Bar");
    EXPECT_EQ(dbusObjs[0].propertyName, "propertyName");
    EXPECT_EQ(dbusObjs[0].propertyType, "string");
}

// TEST(GeneratePDR, testNoJson)
// {
//     ASSERT_THROW(pdr_utils::readJson("./pdr_jsons/not_there"),
//     std::exception);
// }

TEST(GeneratePDR, testMalformedJson)
{
    pdr_utils::Repo pdrRepo = pdr::getRepoByType(
        "./pdr_jsons/state_effecter/good", PLDM_NUMERIC_EFFECTER_PDR);
    ASSERT_EQ(pdrRepo.getRecordCount(), 1);
    ASSERT_THROW(pdr_utils::readJson("./pdr_jsons/state_effecter/malformed"),
                 std::exception);
}
