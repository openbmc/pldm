#include "platform-mc/numeric_sensor.hpp"
#include "platform-mc/terminus.hpp"

#include <libpldm/entity.h>

#include <gtest/gtest.h>

TEST(TerminusTest, supportedTypeTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(1, 1 << PLDM_BASE, event);
    auto t2 = pldm::platform_mc::Terminus(
        2, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);

    EXPECT_EQ(true, t1.doesSupportType(PLDM_BASE));
    EXPECT_EQ(false, t1.doesSupportType(PLDM_PLATFORM));
    EXPECT_EQ(true, t2.doesSupportType(PLDM_BASE));
    EXPECT_EQ(true, t2.doesSupportType(PLDM_PLATFORM));
}

TEST(TerminusTest, getTidTest)
{
    auto event = sdeventplus::Event::get_default();
    const pldm_tid_t tid = 1;
    auto t1 = pldm::platform_mc::Terminus(tid, 1 << PLDM_BASE, event);

    EXPECT_EQ(tid, t1.getTid());
}

TEST(TerminusTest, parseSensorAuxiliaryNamesPDRTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
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

    std::vector<uint8_t> pdr2{
        0x1, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x11,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        3,
        0x80, // entityType system software
        0x1,
        0x0,  // Entity instance number =1
        0,
        0,    // Overall system
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x53, 0x00, 0x30, 0x00,
        0x00  // Entity Name "S0"
    };

    t1.pdrs.emplace_back(pdr1);
    t1.pdrs.emplace_back(pdr2);
    t1.parseTerminusPDRs();

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
    EXPECT_EQ(2, t1.pdrs.size());
    EXPECT_EQ("S0", t1.getTerminusName().value());
}

TEST(TerminusTest, parseSensorAuxiliaryMultiNamesPDRTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
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
        53,                              // dataLength
        0,
        0x0,                             // PLDMTerminusHandle
        0x1,
        0x0,                             // sensorID
        0x1,                             // sensorCount
        0x3,                             // nameStringCount
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
        0x0, // sensorName Temp1
        'f',
        'r',
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
        '2',
        0x0,
        0x0, // sensorName Temp2
        'f',
        'r',
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
        '2',
        0x0,
        0x0 // sensorName Temp12
    };

    std::vector<uint8_t> pdr2{
        0x1, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x11,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        3,
        0x80, // entityType system software
        0x1,
        0x0,  // Entity instance number =1
        0,
        0,    // Overall system
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x53, 0x00, 0x30, 0x00,
        0x00  // Entity Name "S0"
    };

    t1.pdrs.emplace_back(pdr1);
    t1.pdrs.emplace_back(pdr2);
    t1.parseTerminusPDRs();

    auto sensorAuxNames = t1.getSensorAuxiliaryNames(0);
    EXPECT_EQ(nullptr, sensorAuxNames);

    sensorAuxNames = t1.getSensorAuxiliaryNames(1);
    EXPECT_NE(nullptr, sensorAuxNames);

    const auto& [sensorId, sensorCnt, names] = *sensorAuxNames;
    EXPECT_EQ(1, sensorId);
    EXPECT_EQ(1, sensorCnt);
    EXPECT_EQ(1, names.size());
    EXPECT_EQ(3, names[0].size());
    EXPECT_EQ("en", names[0][0].first);
    EXPECT_EQ("TEMP1", names[0][0].second);
    EXPECT_EQ("fr", names[0][1].first);
    EXPECT_EQ("TEMP2", names[0][1].second);
    EXPECT_EQ("fr", names[0][2].first);
    EXPECT_EQ("TEMP12", names[0][2].second);
    EXPECT_EQ(2, t1.pdrs.size());
    EXPECT_EQ("S0", t1.getTerminusName().value());
}

TEST(TerminusTest, parseSensorAuxiliaryNamesMultiSensorsPDRTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
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
        54,                              // dataLength
        0,
        0x0,                             // PLDMTerminusHandle
        0x1,
        0x0,                             // sensorID
        0x2,                             // sensorCount
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
        0x0, // sensorName Temp1
        0x2, // nameStringCount
        'f',
        'r',
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
        '2',
        0x0,
        0x0, // sensorName Temp2
        'f',
        'r',
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
        '2',
        0x0,
        0x0 // sensorName Temp12
    };

    std::vector<uint8_t> pdr2{
        0x1, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x11,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        3,
        0x80, // entityType system software
        0x1,
        0x0,  // Entity instance number =1
        0,
        0,    // Overall system
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x53, 0x00, 0x30, 0x00,
        0x00  // Entity Name "S0"
    };

    t1.pdrs.emplace_back(pdr1);
    t1.pdrs.emplace_back(pdr2);
    t1.parseTerminusPDRs();

    auto sensorAuxNames = t1.getSensorAuxiliaryNames(0);
    EXPECT_EQ(nullptr, sensorAuxNames);

    sensorAuxNames = t1.getSensorAuxiliaryNames(1);
    EXPECT_NE(nullptr, sensorAuxNames);

    const auto& [sensorId, sensorCnt, names] = *sensorAuxNames;
    EXPECT_EQ(1, sensorId);
    EXPECT_EQ(2, sensorCnt);
    EXPECT_EQ(2, names.size());
    EXPECT_EQ(1, names[0].size());
    EXPECT_EQ("en", names[0][0].first);
    EXPECT_EQ("TEMP1", names[0][0].second);
    EXPECT_EQ(2, names[1].size());
    EXPECT_EQ("fr", names[1][0].first);
    EXPECT_EQ("TEMP2", names[1][0].second);
    EXPECT_EQ("fr", names[1][1].first);
    EXPECT_EQ("TEMP12", names[1][1].second);
    EXPECT_EQ(2, t1.pdrs.size());
    EXPECT_EQ("S0", t1.getTerminusName().value());
}

TEST(TerminusTest, parsePDRTestNoSensorPDR)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    std::vector<uint8_t> pdr1{
        0x1, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x11,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        3,
        0x80, // entityType system software
        0x1,
        0x0,  // Entity instance number =1
        0,
        0,    // Overall system
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x53, 0x00, 0x30, 0x00,
        0x00  // Entity Name "S0"
    };

    t1.pdrs.emplace_back(pdr1);
    t1.parseTerminusPDRs();

    auto sensorAuxNames = t1.getSensorAuxiliaryNames(1);
    EXPECT_EQ(nullptr, sensorAuxNames);
}

// ============================================================
// Regression tests for CVE-class bounds-check gaps in
// Terminus::parseSensorAuxiliaryNamesPDR. Each case crafts a
// malicious Sensor Auxiliary Names PDR that, prior to the
// bounds-check fix, drove pointer arithmetic past pdrData's end
// (OOB read from the heap, potential crash). After the fix the
// parser must return nullptr cleanly with no dereference beyond
// the supplied buffer.
// ============================================================

TEST(TerminusTest, parseSensorAuxiliaryNamesPDR_UnterminatedUtf16Rejected)
{
    // Attack: UTF-16 name string in the PDR has no 0x0000
    // terminator before the end of the buffer. The old scan
    //   for (int i = 0; ptr[i] != 0 || ptr[i+1] != 0; i += 2)
    // walks past the buffer into whatever heap memory follows.
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    std::vector<uint8_t> maliciousPdr{
        0x0,
        0x0,
        0x0,
        0x99,                            // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_SENSOR_AUXILIARY_NAMES_PDR, // PDRType
        0x0,
        0x0,                             // recordChangeNumber
        0x0,
        0x0,                             // dataLength
        0x0,
        0x0,                             // terminusHandle
        0x99,
        0x0,                             // sensorID
        0x1,                             // sensorCount
        0x1,                             // nameStringCount
        'e',
        'n',
        0x0, // language tag "en\0"
        'T',
        0x0,
        'E',
        0x0 // UTF-16 "TE" — NO 0x0000 terminator
    };
    t1.pdrs.emplace_back(maliciousPdr);
    EXPECT_NO_THROW(t1.parseTerminusPDRs());
    EXPECT_EQ(nullptr, t1.getSensorAuxiliaryNames(0x99));
}

TEST(TerminusTest, parseSensorAuxiliaryNamesPDR_UnterminatedLangTagRejected)
{
    // Attack: language tag is missing its NUL terminator, so the
    // original std::string_view(const char*) ctor would strlen
    // past the PDR end into heap memory.
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    std::vector<uint8_t> maliciousPdr{
        0x0,
        0x0,
        0x0,
        0x9A,                            // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_SENSOR_AUXILIARY_NAMES_PDR, // PDRType
        0x0,
        0x0,                             // recordChangeNumber
        0x0,
        0x0,                             // dataLength
        0x0,
        0x0,                             // terminusHandle
        0x9A,
        0x0,                             // sensorID
        0x1,                             // sensorCount
        0x1,                             // nameStringCount
        'e',
        'n',
        'U',
        'S' // language tag "enUS" — NO NUL before end
    };
    t1.pdrs.emplace_back(maliciousPdr);
    EXPECT_NO_THROW(t1.parseTerminusPDRs());
    EXPECT_EQ(nullptr, t1.getSensorAuxiliaryNames(0x9A));
}

TEST(TerminusTest, parseSensorAuxiliaryNamesPDR_TruncatedHeaderRejected)
{
    // Attack: PDR too short to even contain the fixed-size prefix
    // (hdr + terminus_handle + sensor_id + sensor_count). The old
    // code cast pdrData.data() to the struct type and dereferenced
    // sensor_count past the end.
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    std::vector<uint8_t> maliciousPdr{
        0x0,
        0x0,
        0x0,
        0x9B,                            // record handle (partial struct)
        0x1,                             // PDRHeaderVersion
        PLDM_SENSOR_AUXILIARY_NAMES_PDR, // PDRType
        0x0,
        0x0                              // recordChangeNumber (and that's all)
    };
    t1.pdrs.emplace_back(maliciousPdr);
    EXPECT_NO_THROW(t1.parseTerminusPDRs());
    EXPECT_EQ(nullptr, t1.getSensorAuxiliaryNames(0));
}

TEST(TerminusTest, parseSensorAuxiliaryNamesPDR_OversizedUtf16Rejected)
{
    // Attack: UTF-16 name string claims to be longer than
    // PLDM_STR_UTF_16_MAX_LEN code units. Must be rejected both
    // as a length-guard (protecting alignedBuffer) and without
    // unbounded scanning.
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);

    std::vector<uint8_t> pdr{
        0x0,
        0x0,
        0x0,
        0x9C,                            // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_SENSOR_AUXILIARY_NAMES_PDR, // PDRType
        0x0,
        0x0,                             // recordChangeNumber
        0x0,
        0x0,                             // dataLength
        0x0,
        0x0,                             // terminusHandle
        0x9C,
        0x0,                             // sensorID
        0x1,                             // sensorCount
        0x1,                             // nameStringCount
        'e',
        'n',
        0x0, // language tag "en\0"
    };
    // Append PLDM_STR_UTF_16_MAX_LEN + 10 non-zero UTF-16 code units
    // (no 0x0000 terminator reached before the length cap fires).
    for (int i = 0; i < PLDM_STR_UTF_16_MAX_LEN + 10; ++i)
    {
        pdr.push_back(0x00);
        pdr.push_back('A'); // any non-zero unit
    }
    // Final 0x0000 terminator so the loop *would* exit if the length
    // cap weren't enforced — ensures the cap, not eventual NUL, is
    // what stops the scan.
    pdr.push_back(0x00);
    pdr.push_back(0x00);

    t1.pdrs.emplace_back(pdr);
    EXPECT_NO_THROW(t1.parseTerminusPDRs());
    EXPECT_EQ(nullptr, t1.getSensorAuxiliaryNames(0x9C));
}
