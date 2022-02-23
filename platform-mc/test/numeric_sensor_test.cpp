
#include "libpldm/entity.h"

#include "platform-mc/numeric_sensor.hpp"
#include "platform-mc/terminus.hpp"

#include <gtest/gtest.h>

using namespace pldm::platform_mc;

TEST(NumericSensor, conversionFormula)
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
        PLDM_SENSOR_UNIT_DEGRESS_C,  // baseUint(2)=degrees C
        0,                           // unitModifier = 0
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
        0xc0,
        0x3f, // resolution=1.5
        0,
        0,
        0x80,
        0x3f, // offset=1.0
        0,
        0, // accuracy
        0, // plusTolerance
        0, // minusTolerance
        2, // hysteresis
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
    auto rc = t1.parsePDRs();
    auto numericSensorPdr = t1.numericSensorPdrs[0];
    EXPECT_EQ(true, rc);
    EXPECT_EQ(1, t1.numericSensorPdrs.size());

    std::string sensorName{"test1"};
    std::string inventoryPath{
        "/xyz/openbmc_project/inventroy/Item/Board/PLDM_device_1"};
    NumericSensor sensor(0x01, 0x01, true, numericSensorPdr, sensorName,
                         inventoryPath);
    auto convertedValue = sensor.conversionFormula(40);
    // (40*1.5 + 1.0 ) * 10^0 = 61
    EXPECT_EQ(61, convertedValue);
}

TEST(NumericSensor, checkThreshold)
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
        PLDM_SENSOR_UNIT_DEGRESS_C,  // baseUint(2)=degrees C
        0,                           // unitModifier = 0
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
        0xc0,
        0x3f, // resolution=1.5
        0,
        0,
        0x80,
        0x3f, // offset=1.0
        0,
        0, // accuracy
        0, // plusTolerance
        0, // minusTolerance
        2, // hysteresis
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
    auto numericSensorPdr = t1.numericSensorPdrs[0];
    std::string sensorName{"test1"};
    std::string inventoryPath{
        "/xyz/openbmc_project/inventroy/Item/Board/PLDM_device_1"};
    NumericSensor sensor(0x01, 0x01, true, numericSensorPdr, sensorName,
                         inventoryPath);

    bool highAlarm = false;
    bool lowAlarm = false;
    double highThreshold = 40;
    double lowThreshold = 30;
    double hysteresis = 2;

    // reading     35->40->45->38->35->30->25->32->35
    // highAlarm    F->T ->T ->T ->F ->F ->F -> F-> F
    // lowAlarm     F->F ->F ->F ->F ->T ->T -> T ->F
    double reading = 35;
    highAlarm = sensor.checkThreshold(highAlarm, true, reading, highThreshold,
                                      hysteresis);
    EXPECT_EQ(false, highAlarm);
    lowAlarm = sensor.checkThreshold(lowAlarm, false, reading, lowThreshold,
                                     hysteresis);
    EXPECT_EQ(false, lowAlarm);

    reading = 40;
    highAlarm = sensor.checkThreshold(highAlarm, true, reading, highThreshold,
                                      hysteresis);
    EXPECT_EQ(true, highAlarm);
    lowAlarm = sensor.checkThreshold(lowAlarm, false, reading, lowThreshold,
                                     hysteresis);
    EXPECT_EQ(false, lowAlarm);

    reading = 45;
    highAlarm = sensor.checkThreshold(highAlarm, true, reading, highThreshold,
                                      hysteresis);
    EXPECT_EQ(true, highAlarm);
    lowAlarm = sensor.checkThreshold(lowAlarm, false, reading, lowThreshold,
                                     hysteresis);
    EXPECT_EQ(false, lowAlarm);

    reading = 38;
    highAlarm = sensor.checkThreshold(highAlarm, true, reading, highThreshold,
                                      hysteresis);
    EXPECT_EQ(true, highAlarm);
    lowAlarm = sensor.checkThreshold(lowAlarm, false, reading, lowThreshold,
                                     hysteresis);
    EXPECT_EQ(false, lowAlarm);

    reading = 35;
    highAlarm = sensor.checkThreshold(highAlarm, true, reading, highThreshold,
                                      hysteresis);
    EXPECT_EQ(false, highAlarm);
    lowAlarm = sensor.checkThreshold(lowAlarm, false, reading, lowThreshold,
                                     hysteresis);
    EXPECT_EQ(false, lowAlarm);

    reading = 30;
    highAlarm = sensor.checkThreshold(highAlarm, true, reading, highThreshold,
                                      hysteresis);
    EXPECT_EQ(false, highAlarm);
    lowAlarm = sensor.checkThreshold(lowAlarm, false, reading, lowThreshold,
                                     hysteresis);
    EXPECT_EQ(true, lowAlarm);

    reading = 25;
    highAlarm = sensor.checkThreshold(highAlarm, true, reading, highThreshold,
                                      hysteresis);
    EXPECT_EQ(false, highAlarm);
    lowAlarm = sensor.checkThreshold(lowAlarm, false, reading, lowThreshold,
                                     hysteresis);
    EXPECT_EQ(true, lowAlarm);

    reading = 32;
    highAlarm = sensor.checkThreshold(highAlarm, true, reading, highThreshold,
                                      hysteresis);
    EXPECT_EQ(false, highAlarm);
    lowAlarm = sensor.checkThreshold(lowAlarm, false, reading, lowThreshold,
                                     hysteresis);
    EXPECT_EQ(true, lowAlarm);

    reading = 35;
    highAlarm = sensor.checkThreshold(highAlarm, true, reading, highThreshold,
                                      hysteresis);
    EXPECT_EQ(false, highAlarm);
    lowAlarm = sensor.checkThreshold(lowAlarm, false, reading, lowThreshold,
                                     hysteresis);
    EXPECT_EQ(false, lowAlarm);
}