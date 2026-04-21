#include "common/utils.hpp"
#include "platform-mc/dbus_impl_fru.hpp"
#include "platform-mc/terminus.hpp"

#include <libpldm/entity.h>
#include <libpldm/fru.h>

#include <sdbusplus/bus.hpp>

#include <string>
#include <utility>
#include <vector>

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

TEST(TerminusTest, createPldmEntityTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string basePath = "/xyz/openbmc_project/inventory/test/";

    // Test all 7 entity type mappings produce non-null entities
    struct EntityTestCase
    {
        uint16_t entityType;
        const char* description;
    };

    // clang-format off
    std::array<EntityTestCase, 10> testCases = {{
        {PLDM_ENTITY_SYSTEM_CHASSIS, "chassis"},
        {PLDM_ENTITY_PROC,           "cpu"},
        {PLDM_ENTITY_MEMORY_MODULE,  "dimm"},
        {PLDM_ENTITY_FAN,            "fan"},
        {PLDM_ENTITY_POWER_SUPPLY,   "powersupply"},
        {PLDM_ENTITY_GPU,            "gpu/accelerator"},
        {PLDM_ENTITY_ACCELERATOR,    "accelerator"},
        {PLDM_ENTITY_BOARD,          "board"},
        {PLDM_ENTITY_SYS_BOARD,      "sysboard/board"},
        {PLDM_ENTITY_CARD,           "card/board"},
    }};
    // clang-format on

    for (size_t i = 0; i < testCases.size(); i++)
    {
        auto path = basePath + std::to_string(i);
        auto entity = pldm::dbus_api::createPldmEntity(bus, path,
                                                       testCases[i].entityType);
        EXPECT_NE(entity, nullptr) << "Failed for " << testCases[i].description;
    }

    // Unknown entity type falls back to Board
    auto fallback =
        pldm::dbus_api::createPldmEntity(bus, basePath + "unknown", 0xFFFF);
    EXPECT_NE(fallback, nullptr) << "Failed for unknown/default entity type";

    // Verify property setters work through PldmFruDecorators
    auto decorators = std::make_unique<pldm::dbus_api::PldmFruDecorators>(
        bus, basePath + "prop_test");
    ASSERT_NE(decorators, nullptr);
    decorators->serialNumber("SN123");
    decorators->partNumber("PN456");
    decorators->manufacturer("TestMfg");
    ASSERT_NE(decorators->getAsset(), nullptr);
    EXPECT_EQ("SN123", decorators->getAsset()->serialNumber());
    EXPECT_EQ("PN456", decorators->getAsset()->partNumber());
    EXPECT_EQ("TestMfg", decorators->getAsset()->manufacturer());
}

// Build one FRU General record that carries the given (type, value) fields
std::vector<uint8_t> buildFruRecord(
    const std::vector<std::pair<uint8_t, std::string>>& fields)
{
    std::vector<uint8_t> record{
        0x1, 0x0,                            // record set identifier
        PLDM_FRU_RECORD_TYPE_GENERAL,        // record type
        static_cast<uint8_t>(fields.size()), // number of fields
        PLDM_FRU_ENCODING_ASCII};            // encoding type
    for (const auto& [type, value] : fields)
    {
        record.push_back(type);
        record.push_back(static_cast<uint8_t>(value.size()));
        record.insert(record.end(), value.begin(), value.end());
    }
    return record;
}

TEST(TerminusTest, fruDecoratorsNotCreatedWithoutFruDataTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    t1.setTerminusName("FruTestNoData");

    // updateInventoryWithFru() is never called for a terminus without FRU
    EXPECT_EQ(nullptr, t1.getFruDecorators());
}

TEST(TerminusTest, fruDecoratorsNotCreatedForNonDecoratorFieldsTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    t1.setTerminusName("FruTestNonDecorator");

    // Vendor and Chassis do not map to any decorator property, and an empty
    // Asset Tag has no value to set
    auto fru = buildFruRecord({{PLDM_FRU_FIELD_TYPE_VENDOR, "Vendor"},
                               {PLDM_FRU_FIELD_TYPE_CHASSIS, "Chassis"},
                               {PLDM_FRU_FIELD_TYPE_ASSET_TAG, ""}});
    t1.updateInventoryWithFru(fru.data(), fru.size());

    auto decorators = t1.getFruDecorators();
    ASSERT_NE(nullptr, decorators);
    EXPECT_EQ(nullptr, decorators->getAsset());
    EXPECT_EQ(nullptr, decorators->getAssetTag());
    EXPECT_EQ(nullptr, decorators->getRevision());
    EXPECT_EQ(nullptr, decorators->getCompatible());
}

TEST(TerminusTest, fruDecoratorsCreatedOnlyForMappedFieldsTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    t1.setTerminusName("FruTestModelOnly");

    auto fru = buildFruRecord({{PLDM_FRU_FIELD_TYPE_MODEL, "Model1"}});
    t1.updateInventoryWithFru(fru.data(), fru.size());

    // Only Decorator.Asset is created, the others stay off the bus
    auto decorators = t1.getFruDecorators();
    ASSERT_NE(nullptr, decorators);
    ASSERT_NE(nullptr, decorators->getAsset());
    EXPECT_EQ("Model1", decorators->getAsset()->model());
    EXPECT_EQ(nullptr, decorators->getAssetTag());
    EXPECT_EQ(nullptr, decorators->getRevision());
    EXPECT_EQ(nullptr, decorators->getCompatible());

    auto t2 = pldm::platform_mc::Terminus(
        2, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    t2.setTerminusName("FruTestPnVersion");

    fru = buildFruRecord(
        {{PLDM_FRU_FIELD_TYPE_PN, "PN2"}, {PLDM_FRU_FIELD_TYPE_VERSION, "B2"}});
    t2.updateInventoryWithFru(fru.data(), fru.size());

    decorators = t2.getFruDecorators();
    ASSERT_NE(nullptr, decorators);
    ASSERT_NE(nullptr, decorators->getAsset());
    EXPECT_EQ("PN2", decorators->getAsset()->partNumber());
    ASSERT_NE(nullptr, decorators->getRevision());
    EXPECT_EQ("B2", decorators->getRevision()->version());
    EXPECT_EQ(nullptr, decorators->getAssetTag());
    EXPECT_EQ(nullptr, decorators->getCompatible());

    auto t3 = pldm::platform_mc::Terminus(
        3, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    t3.setTerminusName("FruTestTagName");

    fru = buildFruRecord({{PLDM_FRU_FIELD_TYPE_ASSET_TAG, "Tag3"},
                          {PLDM_FRU_FIELD_TYPE_NAME, "Name3"}});
    t3.updateInventoryWithFru(fru.data(), fru.size());

    decorators = t3.getFruDecorators();
    ASSERT_NE(nullptr, decorators);
    ASSERT_NE(nullptr, decorators->getAssetTag());
    EXPECT_EQ("Tag3", decorators->getAssetTag()->assetTag());
    ASSERT_NE(nullptr, decorators->getCompatible());
    EXPECT_EQ(std::vector<std::string>{"Name3"},
              decorators->getCompatible()->names());
    EXPECT_EQ(nullptr, decorators->getAsset());
    EXPECT_EQ(nullptr, decorators->getRevision());
}

TEST(TerminusTest, fruDecoratorsCreatedForAllMappedFieldsTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);
    t1.setTerminusName("FruTestAllFields");

    auto fru = buildFruRecord(
        {{PLDM_FRU_FIELD_TYPE_MODEL, "Model1"},
         {PLDM_FRU_FIELD_TYPE_PN, "PN1"},
         {PLDM_FRU_FIELD_TYPE_SN, "SN1"},
         {PLDM_FRU_FIELD_TYPE_MANUFAC, "Mfg1"},
         {PLDM_FRU_FIELD_TYPE_NAME, "Name1"},
         {PLDM_FRU_FIELD_TYPE_VERSION, "B1"},
         {PLDM_FRU_FIELD_TYPE_ASSET_TAG, "Tag1"}});
    t1.updateInventoryWithFru(fru.data(), fru.size());

    auto decorators = t1.getFruDecorators();
    ASSERT_NE(nullptr, decorators);
    ASSERT_NE(nullptr, decorators->getAsset());
    EXPECT_EQ("Model1", decorators->getAsset()->model());
    EXPECT_EQ("PN1", decorators->getAsset()->partNumber());
    EXPECT_EQ("SN1", decorators->getAsset()->serialNumber());
    EXPECT_EQ("Mfg1", decorators->getAsset()->manufacturer());
    ASSERT_NE(nullptr, decorators->getAssetTag());
    EXPECT_EQ("Tag1", decorators->getAssetTag()->assetTag());
    ASSERT_NE(nullptr, decorators->getRevision());
    EXPECT_EQ("B1", decorators->getRevision()->version());
    ASSERT_NE(nullptr, decorators->getCompatible());
    EXPECT_EQ(std::vector<std::string>{"Name1"},
              decorators->getCompatible()->names());
}

TEST(TerminusTest, fruDecoratorsCreationFailureRemovesCreatedTest)
{
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string path = "/xyz/openbmc_project/inventory/test/fru_conflict";

    // The first owner holds Decorator.Revision at the path
    pldm::dbus_api::PldmFruDecorators first(bus, path);
    first.version("B1");
    ASSERT_NE(nullptr, first.getRevision());

    // Decorator.AssetTag is free, so it is created for the second owner
    pldm::dbus_api::PldmFruDecorators second(bus, path);
    second.assetTag("Tag2");
    ASSERT_NE(nullptr, second.getAssetTag());

    // Decorator.Revision is already registered at the path, so creating it
    // fails and the interface created before is removed again
    second.version("B2");
    EXPECT_EQ(nullptr, second.getRevision());
    EXPECT_EQ(nullptr, second.getAssetTag());

    // Nothing is created after the failure
    second.model("Model2");
    EXPECT_EQ(nullptr, second.getAsset());

    // The first owner is not affected
    ASSERT_NE(nullptr, first.getRevision());
    EXPECT_EQ("B1", first.getRevision()->version());
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
