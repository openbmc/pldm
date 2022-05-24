#include "libpldm/entity.h"

#include "platform-mc/terminus.hpp"

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

TEST(TerminusTest, supportedTypeTest)
{
    auto t1 = Terminus(0, 1, 1 << PLDM_BASE);
    auto t2 = Terminus(1, 2, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);

    EXPECT_EQ(true, t1.doesSupport(PLDM_BASE));
    EXPECT_EQ(false, t1.doesSupport(PLDM_PLATFORM));
    EXPECT_EQ(true, t2.doesSupport(PLDM_BASE));
    EXPECT_EQ(true, t2.doesSupport(PLDM_PLATFORM));
}

TEST(TerminusTest, parseSensorAuxiliaryNamesPDRTest)
{
    auto t1 = Terminus(0, 1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
    std::vector<uint8_t> pdr1{
        0x0,
        0x0,
        0x0,
        0x1,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_SENSOR_AUXILIARY_NAMES_PDR, // PDRType
        0x0,
        0x0, // recordChangeNumber
        0x0,
        21, // dataLength
        0,
        0x0, // PLDMTerminusHandle
        0x1,
        0x0, // sensorID
        0x1, // sensorCount
        0x1, // nameStringCount
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
    t1.parsePDRs();
    auto sensorAuxNames = t1.getSensorAuxiliaryNames(1);
    ASSERT_NE(nullptr, sensorAuxNames);

    const auto& [sensorId, sensorCnt, names] = *sensorAuxNames;
    EXPECT_EQ(1, sensorId);
    EXPECT_EQ(1, sensorCnt);
    EXPECT_EQ("en", names[0].first);
    EXPECT_EQ("TEMP1", names[0].second);
}

TEST(TerminusTest, parseNumericSensorPDRTest)
{
    auto t1 = Terminus(0, 1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
    std::vector<uint8_t> pdr1{
        0x0,
        0x0,
        0x0,
        0x1,                     // record handle
        0x1,                     // PDRHeaderVersion
        PLDM_NUMERIC_SENSOR_PDR, // PDRType
        0x0,
        0x0, // recordChangeNumber
        0x0,
        56, // dataLength
        0,
        0, // PLDMTerminusHandle
        0x1,
        0x0, // sensorID=1
        PLDM_ENTITY_POWER_SUPPLY,
        0, // entityType=Power Supply(120)
        1,
        0, // entityInstanceNumber
        0x1,
        0x0,                         // containerID=1
        PLDM_NO_INIT,                // sensorInit
        false,                       // sensorAuxiliaryNamesPDR
        2,                           // baseUint(2)=degrees C
        0,                           // unitModifier
        0,                           // rateUnit
        0,                           // baseOEMUnitHandle
        0,                           // auxUnit
        0,                           // auxUnitModifier
        0,                           // auxRateUnit
        0,                           // rel
        0,                           // auxOEMUnitHandle
        true,                           // isLinear
        PLDM_SENSOR_DATA_SIZE_UINT8, // sensorDataSize
        0,
        0,
        0,
        0, // resolution
        0,
        0,
        0,
        0, // offset
        0,
        0, // accuracy
        0, // plusTolerance
        0, // minusTolerance
        0, // hysteresis
        0, // supportedThresholds
        0, // thresholdAndHysteresisVolatility
        0,
        0,
        0x80,
        0x3f, // stateTransistionInterval=1.0
        0,
        0,
        0x80,
        0x3f,                          // updateInverval=1.0
        255,                           // maxReadable
        0,                             // minReadable
        PLDM_RANGE_FIELD_FORMAT_UINT8, // rangeFieldFormat
        0,                             // rangeFieldsupport
        0,                             // nominalValue
        0,                             // normalMax
        0,                             // normalMin
        0,                             // warningHigh
        0,                             // warningLow
        0,                             // criticalHigh
        0,                             // criticalLow
        0,                             // fatalHigh
        0                              // fatalLow
    };

    t1.pdrs.emplace_back(pdr1);
    t1.parsePDRs();
    EXPECT_EQ(1, t1.numericSensorPdrs.size());

    auto numericSensorPdrs = t1.numericSensorPdrs[0];
    EXPECT_EQ(1, numericSensorPdrs->sensor_id);
    EXPECT_EQ(PLDM_SENSOR_DATA_SIZE_UINT8, numericSensorPdrs->sensor_data_size);
    EXPECT_EQ(PLDM_ENTITY_POWER_SUPPLY, numericSensorPdrs->entity_type);
    EXPECT_EQ(2, numericSensorPdrs->base_unit);
    EXPECT_EQ(0.0, numericSensorPdrs->offset);
    EXPECT_EQ(1.0, numericSensorPdrs->update_interval);
    EXPECT_EQ(255, numericSensorPdrs->max_readable.value_u8);
    EXPECT_EQ(0, numericSensorPdrs->min_readable.value_u8);
    EXPECT_EQ(PLDM_RANGE_FIELD_FORMAT_UINT8,
              numericSensorPdrs->range_field_format);
}