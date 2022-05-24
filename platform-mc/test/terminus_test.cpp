#include "libpldm/entity.h"

#include "platform-mc/terminus.hpp"

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

TEST(TerminusTest, supportedTypeTest)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE);
    auto t2 = Terminus(2, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);

    EXPECT_EQ(true, t1.doesSupport(PLDM_BASE));
    EXPECT_EQ(false, t1.doesSupport(PLDM_PLATFORM));
    EXPECT_EQ(true, t2.doesSupport(PLDM_BASE));
    EXPECT_EQ(true, t2.doesSupport(PLDM_PLATFORM));
}

TEST(TerminusTest, getTidTest)
{
    const pldm_tid_t tid = 1;
    auto t1 = Terminus(tid, 1 << PLDM_BASE);

    EXPECT_EQ(tid, t1.getTid());
}

TEST(TerminusTest, parseSensorAuxiliaryNamesPDRTest)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
    std::vector<uint8_t> pdr1{
        0x0,
        0x0,
        0x0,
        0x1,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_SENSOR_AUXILIARY_NAMES_PDR, // PDRType
        0x0,
        0x0,                             // recordChangeNumber
        0x0,
        21,                              // dataLength
        0,
        0x0,                             // PLDMTerminusHandle
        0x1,
        0x0,                             // sensorID
        0x1,                             // sensorCount
        0x1,                             // nameStringCount
        'e',
        'n',
        0x0, // nameLanguageTag
        0x0,
        'T',
        0x0,
        'E',
        0x0,
        'M',
        0x0,
        'P',
        0x0,
        '1',
        0x0,
        0x0 // sensorName
    };

    t1.pdrs.emplace_back(pdr1);
    auto rc = t1.parsePDRs();
    EXPECT_EQ(true, rc);

    auto sensorAuxNames = t1.getSensorAuxiliaryNames(0);
    EXPECT_EQ(nullptr, sensorAuxNames);

    sensorAuxNames = t1.getSensorAuxiliaryNames(1);
    EXPECT_NE(nullptr, sensorAuxNames);

    const auto& [sensorId, sensorCnt, names] = *sensorAuxNames;
    EXPECT_EQ(1, sensorId);
    EXPECT_EQ(1, sensorCnt);
    EXPECT_EQ(1, names.size());
    EXPECT_EQ(1, names[0].size());
    EXPECT_EQ("en", names[0][0].first);
    EXPECT_EQ("TEMP1", names[0][0].second);
}
