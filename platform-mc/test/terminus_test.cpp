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
    const pldm::tid_t tid = 1;
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

TEST(TerminusTest, parseNumericSensorPDRTest)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
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
        PLDM_SENSOR_UNIT_DEGRESS_C,  // baseUint(2)=degrees C
        0,                           // unitModifier
        0,                           // rateUnit
        0,                           // baseOEMUnitHandle
        0,                           // auxUnit
        0,                           // auxUnitModifier
        0,                           // auxRateUnit
        0,                           // rel
        0,                           // auxOEMUnitHandle
        true,                        // isLinear
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
        3, // hysteresis = 3
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
        50,                            // nominalValue = 50
        60,                            // normalMax = 60
        40,                            // normalMin = 40
        70,                            // warningHigh = 70
        30,                            // warningLow = 30
        80,                            // criticalHigh = 80
        20,                            // criticalLow = 20
        90,                            // fatalHigh = 90
        10                             // fatalLow = 10
    };

    t1.pdrs.emplace_back(pdr1);
    auto rc = t1.parsePDRs();
    EXPECT_EQ(true, rc);
    EXPECT_EQ(1, t1.numericSensorPdrs.size());

    auto numericSensorPdrs = t1.numericSensorPdrs[0];
    EXPECT_EQ(1, numericSensorPdrs->sensor_id);
    EXPECT_EQ(PLDM_SENSOR_DATA_SIZE_UINT8, numericSensorPdrs->sensor_data_size);
    EXPECT_EQ(PLDM_ENTITY_POWER_SUPPLY, numericSensorPdrs->entity_type);
    EXPECT_EQ(2, numericSensorPdrs->base_unit);
    EXPECT_EQ(0.0, numericSensorPdrs->offset);
    EXPECT_EQ(3, numericSensorPdrs->hysteresis.value_u8);
    EXPECT_EQ(1.0, numericSensorPdrs->update_interval);
    EXPECT_EQ(255, numericSensorPdrs->max_readable.value_u8);
    EXPECT_EQ(0, numericSensorPdrs->min_readable.value_u8);
    EXPECT_EQ(PLDM_RANGE_FIELD_FORMAT_UINT8,
              numericSensorPdrs->range_field_format);
    EXPECT_EQ(0, numericSensorPdrs->range_field_support.byte);
    EXPECT_EQ(50, numericSensorPdrs->nominal_value.value_u8);
    EXPECT_EQ(60, numericSensorPdrs->normal_max.value_u8);
    EXPECT_EQ(40, numericSensorPdrs->normal_min.value_u8);
    EXPECT_EQ(70, numericSensorPdrs->warning_high.value_u8);
    EXPECT_EQ(30, numericSensorPdrs->warning_low.value_u8);
    EXPECT_EQ(80, numericSensorPdrs->critical_high.value_u8);
    EXPECT_EQ(20, numericSensorPdrs->critical_low.value_u8);
    EXPECT_EQ(90, numericSensorPdrs->fatal_high.value_u8);
    EXPECT_EQ(10, numericSensorPdrs->fatal_low.value_u8);
}

TEST(TerminusTest, parseNumericSensorPdrSint8Test)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
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
        0x0,                           // containerID=1
        PLDM_NO_INIT,                  // sensorInit
        false,                         // sensorAuxiliaryNamesPDR
        PLDM_SENSOR_UNIT_DEGRESS_C,    // baseUint(2)=degrees C
        0,                             // unitModifier
        0,                             // rateUnit
        0,                             // baseOEMUnitHandle
        0,                             // auxUnit
        0,                             // auxUnitModifier
        0,                             // auxRateUnit
        0,                             // rel
        0,                             // auxOEMUnitHandle
        true,                          // isLinear
        PLDM_RANGE_FIELD_FORMAT_SINT8, // sensorDataSize
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
        3, // hysteresis = 3
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
        0x64,                          // maxReadable = 100
        0x9c,                          // minReadable = -100
        PLDM_RANGE_FIELD_FORMAT_SINT8, // rangeFieldFormat
        0,                             // rangeFieldsupport
        0,                             // nominalValue = 0
        5,                             // normalMax = 5
        0xfb,                          // normalMin = -5
        10,                            // warningHigh = 10
        0xf6,                          // warningLow = -10
        20,                            // criticalHigh = 20
        0xec,                          // criticalLow = -20
        30,                            // fatalHigh = 30
        0xe2                           // fatalLow = -30
    };

    t1.pdrs.emplace_back(pdr1);
    auto rc = t1.parsePDRs();
    EXPECT_EQ(true, rc);
    EXPECT_EQ(1, t1.numericSensorPdrs.size());

    auto numericSensorPdrs = t1.numericSensorPdrs[0];
    EXPECT_EQ(1, numericSensorPdrs->sensor_id);
    EXPECT_EQ(PLDM_SENSOR_DATA_SIZE_SINT8, numericSensorPdrs->sensor_data_size);
    EXPECT_EQ(PLDM_ENTITY_POWER_SUPPLY, numericSensorPdrs->entity_type);
    EXPECT_EQ(2, numericSensorPdrs->base_unit);
    EXPECT_EQ(0.0, numericSensorPdrs->offset);
    EXPECT_EQ(3, numericSensorPdrs->hysteresis.value_s8);
    EXPECT_EQ(1.0, numericSensorPdrs->update_interval);
    EXPECT_EQ(100, numericSensorPdrs->max_readable.value_s8);
    EXPECT_EQ(-100, numericSensorPdrs->min_readable.value_s8);
    EXPECT_EQ(PLDM_RANGE_FIELD_FORMAT_SINT8,
              numericSensorPdrs->range_field_format);
    EXPECT_EQ(0, numericSensorPdrs->range_field_support.byte);
    EXPECT_EQ(0, numericSensorPdrs->nominal_value.value_s8);
    EXPECT_EQ(5, numericSensorPdrs->normal_max.value_s8);
    EXPECT_EQ(-5, numericSensorPdrs->normal_min.value_s8);
    EXPECT_EQ(10, numericSensorPdrs->warning_high.value_s8);
    EXPECT_EQ(-10, numericSensorPdrs->warning_low.value_s8);
    EXPECT_EQ(20, numericSensorPdrs->critical_high.value_s8);
    EXPECT_EQ(-20, numericSensorPdrs->critical_low.value_s8);
    EXPECT_EQ(30, numericSensorPdrs->fatal_high.value_s8);
    EXPECT_EQ(-30, numericSensorPdrs->fatal_low.value_s8);
}

TEST(TerminusTest, parseNumericSensorPdrUint16Test)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
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
        0x0,                          // containerID=1
        PLDM_NO_INIT,                 // sensorInit
        false,                        // sensorAuxiliaryNamesPDR
        PLDM_SENSOR_UNIT_DEGRESS_C,   // baseUint(2)=degrees C
        0,                            // unitModifier
        0,                            // rateUnit
        0,                            // baseOEMUnitHandle
        0,                            // auxUnit
        0,                            // auxUnitModifier
        0,                            // auxRateUnit
        0,                            // rel
        0,                            // auxOEMUnitHandle
        true,                         // isLinear
        PLDM_SENSOR_DATA_SIZE_UINT16, // sensorDataSize
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
        3,
        0, // hysteresis = 3
        0, // supportedThresholds
        0, // thresholdAndHysteresisVolatility
        0,
        0,
        0x80,
        0x3f, // stateTransistionInterval=1.0
        0,
        0,
        0x80,
        0x3f, // updateInverval=1.0
        0,
        0x10, // maxReadable = 4096
        0,
        0,                              // minReadable = 0
        PLDM_RANGE_FIELD_FORMAT_UINT16, // rangeFieldFormat
        0,                              // rangeFieldsupport
        0x88,
        0x13, // nominalValue = 5,000
        0x70,
        0x17, // normalMax = 6,000
        0xa0,
        0x0f, // normalMin = 4,000
        0x58,
        0x1b, // warningHigh = 7,000
        0xb8,
        0x0b, // warningLow = 3,000
        0x40,
        0x1f, // criticalHigh = 8,000
        0xd0,
        0x07, // criticalLow = 2,000
        0x28,
        0x23, // fatalHigh = 9,000
        0xe8,
        0x03 // fatalLow = 1,000
    };

    t1.pdrs.emplace_back(pdr1);
    auto rc = t1.parsePDRs();
    EXPECT_EQ(true, rc);
    EXPECT_EQ(1, t1.numericSensorPdrs.size());

    auto numericSensorPdrs = t1.numericSensorPdrs[0];
    EXPECT_EQ(1, numericSensorPdrs->sensor_id);
    EXPECT_EQ(PLDM_SENSOR_DATA_SIZE_UINT16,
              numericSensorPdrs->sensor_data_size);
    EXPECT_EQ(PLDM_ENTITY_POWER_SUPPLY, numericSensorPdrs->entity_type);
    EXPECT_EQ(2, numericSensorPdrs->base_unit);
    EXPECT_EQ(0.0, numericSensorPdrs->offset);
    EXPECT_EQ(3, numericSensorPdrs->hysteresis.value_u16);
    EXPECT_EQ(1.0, numericSensorPdrs->update_interval);
    EXPECT_EQ(4096, numericSensorPdrs->max_readable.value_u16);
    EXPECT_EQ(0, numericSensorPdrs->min_readable.value_u16);
    EXPECT_EQ(PLDM_RANGE_FIELD_FORMAT_UINT16,
              numericSensorPdrs->range_field_format);
    EXPECT_EQ(0, numericSensorPdrs->range_field_support.byte);
    EXPECT_EQ(5000, numericSensorPdrs->nominal_value.value_u16);
    EXPECT_EQ(6000, numericSensorPdrs->normal_max.value_u16);
    EXPECT_EQ(4000, numericSensorPdrs->normal_min.value_u16);
    EXPECT_EQ(7000, numericSensorPdrs->warning_high.value_u16);
    EXPECT_EQ(3000, numericSensorPdrs->warning_low.value_u16);
    EXPECT_EQ(8000, numericSensorPdrs->critical_high.value_u16);
    EXPECT_EQ(2000, numericSensorPdrs->critical_low.value_u16);
    EXPECT_EQ(9000, numericSensorPdrs->fatal_high.value_u16);
    EXPECT_EQ(1000, numericSensorPdrs->fatal_low.value_u16);
}

TEST(TerminusTest, parseNumericSensorPdrSint16Test)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
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
        0x0,                          // containerID=1
        PLDM_NO_INIT,                 // sensorInit
        false,                        // sensorAuxiliaryNamesPDR
        PLDM_SENSOR_UNIT_DEGRESS_C,   // baseUint(2)=degrees C
        0,                            // unitModifier
        0,                            // rateUnit
        0,                            // baseOEMUnitHandle
        0,                            // auxUnit
        0,                            // auxUnitModifier
        0,                            // auxRateUnit
        0,                            // rel
        0,                            // auxOEMUnitHandle
        true,                         // isLinear
        PLDM_SENSOR_DATA_SIZE_SINT16, // sensorDataSize
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
        3,
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
        0x3f, // updateInverval=1.0
        0xe8,
        0x03, // maxReadable = 1000
        0x18,
        0xfc,                           // minReadable = -1000
        PLDM_RANGE_FIELD_FORMAT_SINT16, // rangeFieldFormat
        0,                              // rangeFieldsupport
        0,
        0, // nominalValue = 0
        0xf4,
        0x01, // normalMax = 500
        0x0c,
        0xfe, // normalMin = -500
        0xe8,
        0x03, // warningHigh = 1,000
        0x18,
        0xfc, // warningLow = -1,000
        0xd0,
        0x07, // criticalHigh = 2,000
        0x30,
        0xf8, // criticalLow = -2,000
        0xb8,
        0x0b, // fatalHigh = 3,000
        0x48,
        0xf4 // fatalLow = -3,000
    };

    t1.pdrs.emplace_back(pdr1);
    auto rc = t1.parsePDRs();
    EXPECT_EQ(true, rc);
    EXPECT_EQ(1, t1.numericSensorPdrs.size());

    auto numericSensorPdrs = t1.numericSensorPdrs[0];
    EXPECT_EQ(1, numericSensorPdrs->sensor_id);
    EXPECT_EQ(PLDM_SENSOR_DATA_SIZE_SINT16,
              numericSensorPdrs->sensor_data_size);
    EXPECT_EQ(PLDM_ENTITY_POWER_SUPPLY, numericSensorPdrs->entity_type);
    EXPECT_EQ(2, numericSensorPdrs->base_unit);
    EXPECT_EQ(0.0, numericSensorPdrs->offset);
    EXPECT_EQ(3, numericSensorPdrs->hysteresis.value_s16);
    EXPECT_EQ(1.0, numericSensorPdrs->update_interval);
    EXPECT_EQ(1000, numericSensorPdrs->max_readable.value_s16);
    EXPECT_EQ(-1000, numericSensorPdrs->min_readable.value_s16);
    EXPECT_EQ(PLDM_RANGE_FIELD_FORMAT_SINT16,
              numericSensorPdrs->range_field_format);
    EXPECT_EQ(0, numericSensorPdrs->range_field_support.byte);
    EXPECT_EQ(0, numericSensorPdrs->nominal_value.value_s16);
    EXPECT_EQ(500, numericSensorPdrs->normal_max.value_s16);
    EXPECT_EQ(-500, numericSensorPdrs->normal_min.value_s16);
    EXPECT_EQ(1000, numericSensorPdrs->warning_high.value_s16);
    EXPECT_EQ(-1000, numericSensorPdrs->warning_low.value_s16);
    EXPECT_EQ(2000, numericSensorPdrs->critical_high.value_s16);
    EXPECT_EQ(-2000, numericSensorPdrs->critical_low.value_s16);
    EXPECT_EQ(3000, numericSensorPdrs->fatal_high.value_s16);
    EXPECT_EQ(-3000, numericSensorPdrs->fatal_low.value_s16);
}

TEST(TerminusTest, parseNumericSensorPdrUint32Test)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
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
        0x0,                          // containerID=1
        PLDM_NO_INIT,                 // sensorInit
        false,                        // sensorAuxiliaryNamesPDR
        PLDM_SENSOR_UNIT_DEGRESS_C,   // baseUint(2)=degrees C
        0,                            // unitModifier
        0,                            // rateUnit
        0,                            // baseOEMUnitHandle
        0,                            // auxUnit
        0,                            // auxUnitModifier
        0,                            // auxRateUnit
        0,                            // rel
        0,                            // auxOEMUnitHandle
        true,                         // isLinear
        PLDM_SENSOR_DATA_SIZE_UINT32, // sensorDataSize
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
        3,
        0,
        0,
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
        0x3f, // updateInverval=1.0
        0,
        0x10,
        0,
        0, // maxReadable = 4096
        0,
        0,
        0,
        0,                              // minReadable = 0
        PLDM_RANGE_FIELD_FORMAT_UINT32, // rangeFieldFormat
        0,                              // rangeFieldsupport
        0x40,
        0x4b,
        0x4c,
        0x00, // nominalValue = 5,000,000
        0x80,
        0x8d,
        0x5b,
        0x00, // normalMax = 6,000,000
        0x00,
        0x09,
        0x3d,
        0x00, // normalMin = 4,000,000
        0xc0,
        0xcf,
        0x6a,
        0x00, // warningHigh = 7,000,000
        0xc0,
        0xc6,
        0x2d,
        0x00, // warningLow = 3,000,000
        0x00,
        0x12,
        0x7a,
        0x00, // criticalHigh = 8,000,000
        0x80,
        0x84,
        0x1e,
        0x00, // criticalLow = 2,000,000
        0x40,
        0x54,
        0x89,
        0x00, // fatalHigh = 9,000,000
        0x40,
        0x42,
        0x0f,
        0x00 // fatalLow = 1,000,000
    };

    t1.pdrs.emplace_back(pdr1);
    auto rc = t1.parsePDRs();
    EXPECT_EQ(true, rc);
    EXPECT_EQ(1, t1.numericSensorPdrs.size());

    auto numericSensorPdrs = t1.numericSensorPdrs[0];
    EXPECT_EQ(1, numericSensorPdrs->sensor_id);
    EXPECT_EQ(PLDM_SENSOR_DATA_SIZE_UINT32,
              numericSensorPdrs->sensor_data_size);
    EXPECT_EQ(PLDM_ENTITY_POWER_SUPPLY, numericSensorPdrs->entity_type);
    EXPECT_EQ(2, numericSensorPdrs->base_unit);
    EXPECT_EQ(0.0, numericSensorPdrs->offset);
    EXPECT_EQ(3, numericSensorPdrs->hysteresis.value_u32);
    EXPECT_EQ(1.0, numericSensorPdrs->update_interval);
    EXPECT_EQ(4096, numericSensorPdrs->max_readable.value_u32);
    EXPECT_EQ(0, numericSensorPdrs->min_readable.value_u32);
    EXPECT_EQ(PLDM_RANGE_FIELD_FORMAT_UINT32,
              numericSensorPdrs->range_field_format);
    EXPECT_EQ(0, numericSensorPdrs->range_field_support.byte);
    EXPECT_EQ(5000000, numericSensorPdrs->nominal_value.value_u32);
    EXPECT_EQ(6000000, numericSensorPdrs->normal_max.value_u32);
    EXPECT_EQ(4000000, numericSensorPdrs->normal_min.value_u32);
    EXPECT_EQ(7000000, numericSensorPdrs->warning_high.value_u32);
    EXPECT_EQ(3000000, numericSensorPdrs->warning_low.value_u32);
    EXPECT_EQ(8000000, numericSensorPdrs->critical_high.value_u32);
    EXPECT_EQ(2000000, numericSensorPdrs->critical_low.value_u32);
    EXPECT_EQ(9000000, numericSensorPdrs->fatal_high.value_u32);
    EXPECT_EQ(1000000, numericSensorPdrs->fatal_low.value_u32);
}

TEST(TerminusTest, parseNumericSensorPdrSint32Test)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
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
        0x0,                          // containerID=1
        PLDM_NO_INIT,                 // sensorInit
        false,                        // sensorAuxiliaryNamesPDR
        PLDM_SENSOR_UNIT_DEGRESS_C,   // baseUint(2)=degrees C
        0,                            // unitModifier
        0,                            // rateUnit
        0,                            // baseOEMUnitHandle
        0,                            // auxUnit
        0,                            // auxUnitModifier
        0,                            // auxRateUnit
        0,                            // rel
        0,                            // auxOEMUnitHandle
        true,                         // isLinear
        PLDM_SENSOR_DATA_SIZE_SINT32, // sensorDataSize
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
        3,
        0,
        0,
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
        0x3f, // updateInverval=1.0
        0xa0,
        0x86,
        0x01,
        0x00, // maxReadable = 100000
        0x60,
        0x79,
        0xfe,
        0xff,                           // minReadable = -10000
        PLDM_RANGE_FIELD_FORMAT_SINT32, // rangeFieldFormat
        0,                              // rangeFieldsupport
        0,
        0,
        0,
        0, // nominalValue = 0
        0x20,
        0xa1,
        0x07,
        0x00, // normalMax = 500,000
        0xe0,
        0x5e,
        0xf8,
        0xff, // normalMin = -500,000
        0x40,
        0x42,
        0x0f,
        0x00, // warningHigh = 1,000,000
        0xc0,
        0xbd,
        0xf0,
        0xff, // warningLow = -1,000,000
        0x80,
        0x84,
        0x1e,
        0x00, // criticalHigh = 2,000,000
        0x80,
        0x7b,
        0xe1,
        0xff, // criticalLow = -2,000,000
        0xc0,
        0xc6,
        0x2d,
        0x00, // fatalHigh = 3,000,000
        0x40,
        0x39,
        0xd2,
        0xff // fatalLow = -3,000,000
    };

    t1.pdrs.emplace_back(pdr1);
    auto rc = t1.parsePDRs();
    EXPECT_EQ(true, rc);
    EXPECT_EQ(1, t1.numericSensorPdrs.size());

    auto numericSensorPdrs = t1.numericSensorPdrs[0];
    EXPECT_EQ(1, numericSensorPdrs->sensor_id);
    EXPECT_EQ(PLDM_SENSOR_DATA_SIZE_SINT32,
              numericSensorPdrs->sensor_data_size);
    EXPECT_EQ(PLDM_ENTITY_POWER_SUPPLY, numericSensorPdrs->entity_type);
    EXPECT_EQ(2, numericSensorPdrs->base_unit);
    EXPECT_EQ(0.0, numericSensorPdrs->offset);
    EXPECT_EQ(3, numericSensorPdrs->hysteresis.value_s32);
    EXPECT_EQ(1.0, numericSensorPdrs->update_interval);
    EXPECT_EQ(100000, numericSensorPdrs->max_readable.value_s32);
    EXPECT_EQ(-100000, numericSensorPdrs->min_readable.value_s32);
    EXPECT_EQ(PLDM_RANGE_FIELD_FORMAT_SINT32,
              numericSensorPdrs->range_field_format);
    EXPECT_EQ(0, numericSensorPdrs->range_field_support.byte);
    EXPECT_EQ(0, numericSensorPdrs->nominal_value.value_s32);
    EXPECT_EQ(500000, numericSensorPdrs->normal_max.value_s32);
    EXPECT_EQ(-500000, numericSensorPdrs->normal_min.value_s32);
    EXPECT_EQ(1000000, numericSensorPdrs->warning_high.value_s32);
    EXPECT_EQ(-1000000, numericSensorPdrs->warning_low.value_s32);
    EXPECT_EQ(2000000, numericSensorPdrs->critical_high.value_s32);
    EXPECT_EQ(-2000000, numericSensorPdrs->critical_low.value_s32);
    EXPECT_EQ(3000000, numericSensorPdrs->fatal_high.value_s32);
    EXPECT_EQ(-3000000, numericSensorPdrs->fatal_low.value_s32);
}

TEST(TerminusTest, parseNumericSensorPdrReal32Test)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
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
        0x0,                          // containerID=1
        PLDM_NO_INIT,                 // sensorInit
        false,                        // sensorAuxiliaryNamesPDR
        PLDM_SENSOR_UNIT_DEGRESS_C,   // baseUint(2)=degrees C
        0,                            // unitModifier
        0,                            // rateUnit
        0,                            // baseOEMUnitHandle
        0,                            // auxUnit
        0,                            // auxUnitModifier
        0,                            // auxRateUnit
        0,                            // rel
        0,                            // auxOEMUnitHandle
        true,                         // isLinear
        PLDM_SENSOR_DATA_SIZE_SINT32, // sensorDataSize
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
        3,
        0,
        0,
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
        0x3f, // updateInverval=1.0
        0xa0,
        0x86,
        0x01,
        0x00, // maxReadable = 100000
        0x60,
        0x79,
        0xfe,
        0xff,                           // minReadable = -10000
        PLDM_RANGE_FIELD_FORMAT_REAL32, // rangeFieldFormat
        0,                              // rangeFieldsupport
        0,
        0,
        0,
        0, // nominalValue = 0.0
        0x33,
        0x33,
        0x48,
        0x42, // normalMax = 50.05
        0x33,
        0x33,
        0x48,
        0xc2, // normalMin = -50.05
        0x83,
        0x00,
        0xc8,
        0x42, // warningHigh = 100.001
        0x83,
        0x00,
        0xc8,
        0xc2, // warningLow = -100.001
        0x83,
        0x00,
        0x48,
        0x43, // criticalHigh = 200.002
        0x83,
        0x00,
        0x48,
        0xc3, // criticalLow = -200.002
        0x62,
        0x00,
        0x96,
        0x43, // fatalHigh = 300.003
        0x62,
        0x00,
        0x96,
        0xc3 // fatalLow = -300.003
    };

    t1.pdrs.emplace_back(pdr1);
    auto rc = t1.parsePDRs();
    EXPECT_EQ(true, rc);
    EXPECT_EQ(1, t1.numericSensorPdrs.size());

    auto numericSensorPdrs = t1.numericSensorPdrs[0];
    EXPECT_EQ(1, numericSensorPdrs->sensor_id);
    EXPECT_EQ(PLDM_SENSOR_DATA_SIZE_SINT32,
              numericSensorPdrs->sensor_data_size);
    EXPECT_EQ(PLDM_ENTITY_POWER_SUPPLY, numericSensorPdrs->entity_type);
    EXPECT_EQ(2, numericSensorPdrs->base_unit);
    EXPECT_EQ(0.0, numericSensorPdrs->offset);
    EXPECT_EQ(3, numericSensorPdrs->hysteresis.value_s32);
    EXPECT_EQ(1.0, numericSensorPdrs->update_interval);
    EXPECT_EQ(100000, numericSensorPdrs->max_readable.value_s32);
    EXPECT_EQ(-100000, numericSensorPdrs->min_readable.value_s32);
    EXPECT_EQ(PLDM_RANGE_FIELD_FORMAT_REAL32,
              numericSensorPdrs->range_field_format);
    EXPECT_FLOAT_EQ(0, numericSensorPdrs->range_field_support.byte);
    EXPECT_FLOAT_EQ(0, numericSensorPdrs->nominal_value.value_f32);
    EXPECT_FLOAT_EQ(50.05f, numericSensorPdrs->normal_max.value_f32);
    EXPECT_FLOAT_EQ(-50.05f, numericSensorPdrs->normal_min.value_f32);
    EXPECT_FLOAT_EQ(100.001f, numericSensorPdrs->warning_high.value_f32);
    EXPECT_FLOAT_EQ(-100.001f, numericSensorPdrs->warning_low.value_f32);
    EXPECT_FLOAT_EQ(200.002f, numericSensorPdrs->critical_high.value_f32);
    EXPECT_FLOAT_EQ(-200.002f, numericSensorPdrs->critical_low.value_f32);
    EXPECT_FLOAT_EQ(300.003f, numericSensorPdrs->fatal_high.value_f32);
    EXPECT_FLOAT_EQ(-300.003f, numericSensorPdrs->fatal_low.value_f32);
}

TEST(TerminusTest, parseNumericSensorPDRInvalidSizeTest)
{
    auto t1 = Terminus(1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM);
    // A corrupted PDR. The data after plusTolerance missed.
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
        true,                        // isLinear
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
        0  // plusTolerance
    };

    t1.pdrs.emplace_back(pdr1);
    auto rc = t1.parsePDRs();
    EXPECT_EQ(true, rc);
    EXPECT_EQ(0, t1.numericSensorPdrs.size());
}
