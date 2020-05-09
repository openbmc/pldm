#include "libpldmresponder/pdr_utils.hpp"
#include "libpldmresponder/platform.hpp"

#include "libpldm/platform.h"

#include <gtest/gtest.h>

using namespace pldm::responder;
using namespace pldm::responder::platform;
using namespace pldm::responder::pdr;
using namespace pldm::responder::pdr_utils;

TEST(GeneratePDRByStateSensor, testGoodJson)
{
    auto inPDRRepo = pldm_pdr_init();
    auto outPDRRepo = pldm_pdr_init();
    Repo outRepo(outPDRRepo);
    Handler handler("./pdr_jsons/state_sensor/good", inPDRRepo, nullptr);
    Repo inRepo(inPDRRepo);
    getRepoByType(inRepo, outRepo, PLDM_STATE_SENSOR_PDR);

    // 1 entries
    ASSERT_EQ(outRepo.getRecordCount(), 1);

    // Check first PDR
    pdr_utils::PdrEntry e;
    auto record = pdr::getRecordByHandle(outRepo, 1, e);
    ASSERT_NE(record, nullptr);

    pldm_state_sensor_pdr* pdr =
        reinterpret_cast<pldm_state_sensor_pdr*>(e.data);
    EXPECT_EQ(pdr->hdr.record_handle, 1);
    EXPECT_EQ(pdr->hdr.version, 1);
    EXPECT_EQ(pdr->hdr.type, PLDM_STATE_SENSOR_PDR);
    EXPECT_EQ(pdr->hdr.record_change_num, 0);
    EXPECT_EQ(pdr->hdr.length, 17);

    EXPECT_EQ(pdr->sensor_id, 1);

    const auto& [dbusMappings, dbusValMaps] =
        handler.getDbusObjMaps(pdr->sensor_id, PLDM_SENSOR_ID);
    ASSERT_EQ(dbusMappings[0].objectPath, "/foo/bar");
    ASSERT_EQ(dbusMappings[0].interface, "xyz.openbmc_project.Foo.Bar");
    ASSERT_EQ(dbusMappings[0].propertyName, "propertyName");
    ASSERT_EQ(dbusMappings[0].propertyType, "string");

    pldm_pdr_destroy(inPDRRepo);
    pldm_pdr_destroy(outPDRRepo);
}

TEST(GeneratePDR, testNoJson)
{
    auto pdrRepo = pldm_pdr_init();

    ASSERT_THROW(Handler("./pdr_jsons/not_there", pdrRepo, nullptr),
                 std::exception);

    pldm_pdr_destroy(pdrRepo);
}

TEST(GeneratePDR, testMalformedJson)
{
    auto inPDRRepo = pldm_pdr_init();
    auto outPDRRepo = pldm_pdr_init();
    Repo outRepo(outPDRRepo);
    Handler handler("./pdr_jsons/state_sensor/good", inPDRRepo, nullptr);
    Repo inRepo(inPDRRepo);
    getRepoByType(inRepo, outRepo, PLDM_STATE_SENSOR_PDR);

    ASSERT_EQ(outRepo.getRecordCount(), 1);
    ASSERT_THROW(pdr_utils::readJson("./pdr_jsons/state_sensor/malformed"),
                 std::exception);

    pldm_pdr_destroy(inPDRRepo);
    pldm_pdr_destroy(outPDRRepo);
}
