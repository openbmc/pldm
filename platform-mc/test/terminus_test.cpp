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
    EXPECT_EQ("S0_1", t1.getTerminusName().value());
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
    EXPECT_EQ("S0_1", t1.getTerminusName().value());
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
    EXPECT_EQ("S0_1", t1.getTerminusName().value());
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
    std::array<EntityTestCase, 13> testCases = {{
        {PLDM_ENTITY_SYSTEM_CHASSIS, "chassis"},
        {PLDM_ENTITY_PROC,           "cpu"},
        {PLDM_ENTITY_MEMORY_MODULE,  "dimm"},
        {PLDM_ENTITY_FAN,            "fan"},
        {PLDM_ENTITY_POWER_SUPPLY,   "powersupply"},
        {PLDM_ENTITY_GPU,            "gpu/accelerator"},
        {PLDM_ENTITY_ACCELERATOR,    "accelerator"},
        {PLDM_ENTITY_NETWORK_CONTROLLER, "networkadapter"},
        {PLDM_ENTITY_OSFP,           "connector"},
        {PLDM_ENTITY_ETHERNET,       "ethernet"},
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

TEST(TerminusTest, parseEntityAssociationPDRTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);

    constexpr uint16_t terminusEntityType = 0x8003;
    constexpr uint16_t unmappedEntityType = 0xFFFF;

    std::vector<uint8_t> entityAuxNamesPdr{
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

    std::vector<uint8_t> procAuxNamesPdr{
        0x2, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x15,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        PLDM_ENTITY_PROC,
        0x0,  // entityType processor
        0x1,
        0x0,  // Entity instance number = 1
        0x1,
        0x0,  // Container ID = 1
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x43, 0x00, 0x70, 0x00, 0x75, 0x00, 0x30, 0x00,
        0x00  // Entity Name "Cpu0"
    };

    std::vector<uint8_t> fanAuxNamesPdr{
        0x6, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x15,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        PLDM_ENTITY_FAN,
        0x0,  // entityType fan
        0x1,
        0x0,  // Entity instance number = 1
        0x1,
        0x0,  // Container ID = 1
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x46, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x30, 0x00,
        0x00  // Entity Name "Fan0"
    };

    std::vector<uint8_t> entityAssociationPdr{
        0x3, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x1c,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x1,
        0x0,  // containerID = 1
        0x0,  // associationType physical
        3,
        0x80, // container entityType system software
        0x1,
        0x0,  // container entity instance number = 1
        0x0,
        0x0,  // container entity container ID = 0
        3,    // numberOfChildren
        PLDM_ENTITY_PROC,
        0x0,  // child entityType processor
        0x1,
        0x0,  // child entity instance number = 1
        0x1,
        0x0,  // child entity container ID = 1
        PLDM_ENTITY_PROC,
        0x0,  // child entityType processor
        0x2,
        0x0,  // child entity instance number = 2
        0x1,
        0x0,  // child entity container ID = 1
        0xff,
        0xff, // child entityType with no Item interface
        0x1,
        0x0,  // child entity instance number = 1
        0x1,
        0x0   // child entity container ID = 1
    };

    std::vector<uint8_t> otherContainerPdr{
        0x4, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x10,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x2,
        0x0,  // containerID = 2
        0x0,  // associationType physical
        3,
        0x80, // container entityType system software
        0x1,
        0x0,  // container entity instance number = 1
        0x0,
        0x0,  // container entity container ID = 0
        1,    // numberOfChildren
        PLDM_ENTITY_PROC,
        0x0,  // child entityType processor
        0x2,
        0x0,  // child entity instance number = 2
        0x2,
        0x0   // child entity container ID = 2
    };

    std::vector<uint8_t> unmappedContainerPdr{
        0x5, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x16,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x3,
        0x0,  // containerID = 3
        0x0,  // associationType physical
        0xff,
        0xff, // container entityType with no Item interface
        0x1,
        0x0,  // container entity instance number = 1
        0x1,
        0x0,  // container entity container ID = 1
        2,    // numberOfChildren
        PLDM_ENTITY_PROC,
        0x0,  // child entityType processor
        0x1,
        0x0,  // child entity instance number = 1
        0x3,
        0x0,  // child entity container ID = 3
        PLDM_ENTITY_MEMORY_MODULE,
        0x0,  // child entityType memory module
        0x1,
        0x0,  // child entity instance number = 1
        0x3,
        0x0   // child entity container ID = 3
    };

    t1.pdrs.emplace_back(entityAuxNamesPdr);
    t1.pdrs.emplace_back(procAuxNamesPdr);
    t1.pdrs.emplace_back(fanAuxNamesPdr);
    t1.pdrs.emplace_back(entityAssociationPdr);
    t1.pdrs.emplace_back(otherContainerPdr);
    t1.pdrs.emplace_back(unmappedContainerPdr);
    t1.parseTerminusPDRs();

    EXPECT_EQ("S0", t1.getTerminusName().value());

    /* The overall terminus entity is exposed by the terminus inventory path */
    EXPECT_EQ(nullptr, t1.getEntity({terminusEntityType, 1, 0}));

    /* The Entity Auxiliary Names PDR names the entity object */
    auto namedEntity = t1.getEntity({PLDM_ENTITY_PROC, 1, 1});
    ASSERT_NE(nullptr, namedEntity);
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/S0_Cpu0",
              namedEntity->getPath());

    /* An unnamed entity is named after the terminus ID and the entity
     * identification fields
     */
    auto unnamedEntity = t1.getEntity({PLDM_ENTITY_PROC, 2, 1});
    ASSERT_NE(nullptr, unnamedEntity);
    EXPECT_EQ(
        "/xyz/openbmc_project/inventory/system/Terminus_1_Container_1_Cpu_2",
        unnamedEntity->getPath());

    /* An entity instance number is unique within the container of the entity,
     * so two entities of the same type and instance number in two containers
     * are two objects
     */
    auto otherContainerEntity = t1.getEntity({PLDM_ENTITY_PROC, 2, 2});
    ASSERT_NE(nullptr, otherContainerEntity);
    EXPECT_EQ(
        "/xyz/openbmc_project/inventory/system/Terminus_1_Container_2_Cpu_2",
        otherContainerEntity->getPath());

    /* An entity type with no Inventory Item interface is not exposed */
    EXPECT_EQ(nullptr, t1.getEntity({unmappedEntityType, 1, 1}));

    /* An entity which no Entity Association PDR records is not exposed, even
     * when an Entity Auxiliary Names PDR names it
     */
    EXPECT_EQ(nullptr, t1.getEntity({PLDM_ENTITY_FAN, 1, 1}));

    /* The container is the terminus inventory path */
    std::tuple<std::string, std::string, std::string> container{
        "contained_by", "containing",
        "/xyz/openbmc_project/inventory/system/S0"};
    EXPECT_EQ(container, namedEntity->getContainer());
    EXPECT_EQ(container, unnamedEntity->getContainer());

    /* A container with no entity D-Bus object hides the entities it contains
     */
    auto hiddenProcEntity = t1.getEntity({PLDM_ENTITY_PROC, 1, 3});
    ASSERT_NE(nullptr, hiddenProcEntity);
    EXPECT_FALSE(hiddenProcEntity->getContainer().has_value());

    auto hiddenDimmEntity = t1.getEntity({PLDM_ENTITY_MEMORY_MODULE, 1, 3});
    ASSERT_NE(nullptr, hiddenDimmEntity);
    EXPECT_FALSE(hiddenDimmEntity->getContainer().has_value());

    /* A sensor of an entity is associated to the entity D-Bus object */
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/S0_Cpu0",
              t1.getEntityInventoryPath({PLDM_ENTITY_PROC, 1, 1}));

    /* A sensor of an entity with no D-Bus object is associated to the
     * terminus inventory path
     */
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/S0",
              t1.getEntityInventoryPath({terminusEntityType, 1, 0}));
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/S0",
              t1.getEntityInventoryPath({unmappedEntityType, 1, 1}));
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/S0",
              t1.getEntityInventoryPath({PLDM_ENTITY_FAN, 1, 1}));
}

TEST(TerminusTest, duplicateEntityNameTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);

    std::vector<uint8_t> entityAuxNamesPdr{
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

    std::vector<uint8_t> firstProcAuxNamesPdr{
        0x2, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x15,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        PLDM_ENTITY_PROC,
        0x0,  // entityType processor
        0x1,
        0x0,  // Entity instance number = 1
        0x1,
        0x0,  // Container ID = 1
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x43, 0x00, 0x70, 0x00, 0x75, 0x00, 0x30, 0x00,
        0x00  // Entity Name "Cpu0"
    };

    std::vector<uint8_t> secondProcAuxNamesPdr{
        0x3, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x15,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        PLDM_ENTITY_PROC,
        0x0,  // entityType processor
        0x2,
        0x0,  // Entity instance number = 2
        0x1,
        0x0,  // Container ID = 1
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x43, 0x00, 0x70, 0x00, 0x75, 0x00, 0x30, 0x00,
        0x00  // Entity Name "Cpu0"
    };

    std::vector<uint8_t> thirdProcAuxNamesPdr{
        0x4, 0x0, 0x0,
        0x0,                             // record handle
        0x1,                             // PDRHeaderVersion
        PLDM_ENTITY_AUXILIARY_NAMES_PDR, // PDRType
        0x1,
        0x0,                             // recordChangeNumber
        0x15,
        0,                               // dataLength
        /* Entity Auxiliary Names PDR Data*/
        PLDM_ENTITY_PROC,
        0x0,  // entityType processor
        0x3,
        0x0,  // Entity instance number = 3
        0x1,
        0x0,  // Container ID = 1
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x43, 0x00, 0x70, 0x00, 0x75, 0x00, 0x31, 0x00,
        0x00  // Entity Name "Cpu1"
    };

    std::vector<uint8_t> entityAssociationPdr{
        0x5, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x1c,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x1,
        0x0,  // containerID = 1
        0x0,  // associationType physical
        3,
        0x80, // container entityType system software
        0x1,
        0x0,  // container entity instance number = 1
        0x0,
        0x0,  // container entity container ID = 0
        3,    // numberOfChildren
        PLDM_ENTITY_PROC,
        0x0,  // child entityType processor
        0x1,
        0x0,  // child entity instance number = 1
        0x1,
        0x0,  // child entity container ID = 1
        PLDM_ENTITY_PROC,
        0x0,  // child entityType processor
        0x2,
        0x0,  // child entity instance number = 2
        0x1,
        0x0,  // child entity container ID = 1
        PLDM_ENTITY_PROC,
        0x0,  // child entityType processor
        0x3,
        0x0,  // child entity instance number = 3
        0x1,
        0x0   // child entity container ID = 1
    };

    t1.pdrs.emplace_back(entityAuxNamesPdr);
    t1.pdrs.emplace_back(firstProcAuxNamesPdr);
    t1.pdrs.emplace_back(secondProcAuxNamesPdr);
    t1.pdrs.emplace_back(thirdProcAuxNamesPdr);
    t1.pdrs.emplace_back(entityAssociationPdr);
    t1.parseTerminusPDRs();

    /* Entities which share an entity name are named after the terminus ID
     * and the entity identification fields
     */
    auto firstEntity = t1.getEntity({PLDM_ENTITY_PROC, 1, 1});
    ASSERT_NE(nullptr, firstEntity);
    EXPECT_EQ(
        "/xyz/openbmc_project/inventory/system/Terminus_1_Container_1_Cpu_1",
        firstEntity->getPath());

    auto secondEntity = t1.getEntity({PLDM_ENTITY_PROC, 2, 1});
    ASSERT_NE(nullptr, secondEntity);
    EXPECT_EQ(
        "/xyz/openbmc_project/inventory/system/Terminus_1_Container_1_Cpu_2",
        secondEntity->getPath());

    /* An entity name which no other entity has names the entity object */
    auto thirdEntity = t1.getEntity({PLDM_ENTITY_PROC, 3, 1});
    ASSERT_NE(nullptr, thirdEntity);
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/S0_Cpu1",
              thirdEntity->getPath());
}

TEST(TerminusTest, terminusEntityWithoutAuxNamesTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);

    /* The system board in the system container is the overall terminus
     * entity, which no Entity Auxiliary Names PDR names
     */
    std::vector<uint8_t> physicalPdr{
        0x1, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x16,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x1,
        0x0, // containerID = 1
        0x0, // associationType physical
        PLDM_ENTITY_SYS_BOARD,
        0x0, // container entityType system board
        0x1,
        0x0, // container entity instance number = 1
        0x0,
        0x0, // container entity container ID = 0
        2,   // numberOfChildren
        PLDM_ENTITY_PROC,
        0x0, // child entityType processor
        0x1,
        0x0, // child entity instance number = 1
        0x1,
        0x0, // child entity container ID = 1
        PLDM_ENTITY_BOARD,
        0x0, // child entityType board
        0x1,
        0x0, // child entity instance number = 1
        0x1,
        0x0  // child entity container ID = 1
    };

    std::vector<uint8_t> logicalPdr{
        0x2, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x10,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x2,
        0x0, // containerID = 2
        0x1, // associationType logical
        PLDM_ENTITY_BOARD,
        0x0, // container entityType board
        0x1,
        0x0, // container entity instance number = 1
        0x1,
        0x0, // container entity container ID = 1
        1,   // numberOfChildren
        PLDM_ENTITY_PROC,
        0x0, // child entityType processor
        0x1,
        0x0, // child entity instance number = 1
        0x1,
        0x0  // child entity container ID = 1
    };

    std::vector<uint8_t> secondPhysicalPdr{
        0x3, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x10,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x3,
        0x0, // containerID = 3
        0x0, // associationType physical
        PLDM_ENTITY_BOARD,
        0x0, // container entityType board
        0x1,
        0x0, // container entity instance number = 1
        0x1,
        0x0, // container entity container ID = 1
        1,   // numberOfChildren
        PLDM_ENTITY_PROC,
        0x0, // child entityType processor
        0x1,
        0x0, // child entity instance number = 1
        0x1,
        0x0  // child entity container ID = 1
    };

    t1.pdrs.emplace_back(physicalPdr);
    t1.pdrs.emplace_back(logicalPdr);
    t1.pdrs.emplace_back(secondPhysicalPdr);
    t1.parseTerminusPDRs();

    /* The overall terminus entity is exposed by the terminus inventory path */
    EXPECT_EQ(nullptr, t1.getEntity({PLDM_ENTITY_SYS_BOARD, 1, 0}));

    std::tuple<std::string, std::string, std::string> container{
        "contained_by", "containing",
        "/xyz/openbmc_project/inventory/system/Terminus_1"};

    auto board = t1.getEntity({PLDM_ENTITY_BOARD, 1, 1});
    ASSERT_NE(nullptr, board);
    EXPECT_EQ(container, board->getContainer());

    /* The logical association and the second physical container do not
     * replace the physical container of the entity
     */
    auto proc = t1.getEntity({PLDM_ENTITY_PROC, 1, 1});
    ASSERT_NE(nullptr, proc);
    EXPECT_EQ(container, proc->getContainer());
}

TEST(TerminusTest, nestedEntityContainerTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);

    /* The system board is the overall terminus entity, it contains a card
     * which contains a processor
     */
    std::vector<uint8_t> boardPdr{
        0x1, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x10,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x1,
        0x0, // containerID = 1
        0x0, // associationType physical
        PLDM_ENTITY_SYS_BOARD,
        0x0, // container entityType system board
        0x1,
        0x0, // container entity instance number = 1
        0x0,
        0x0, // container entity container ID = 0
        1,   // numberOfChildren
        PLDM_ENTITY_CARD,
        0x0, // child entityType card
        0x1,
        0x0, // child entity instance number = 1
        0x1,
        0x0  // child entity container ID = 1
    };

    std::vector<uint8_t> cardPdr{
        0x2, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x10,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x2,
        0x0, // containerID = 2
        0x0, // associationType physical
        PLDM_ENTITY_CARD,
        0x0, // container entityType card
        0x1,
        0x0, // container entity instance number = 1
        0x1,
        0x0, // container entity container ID = 1
        1,   // numberOfChildren
        PLDM_ENTITY_PROC,
        0x0, // child entityType processor
        0x1,
        0x0, // child entity instance number = 1
        0x2,
        0x0  // child entity container ID = 2
    };

    t1.pdrs.emplace_back(boardPdr);
    t1.pdrs.emplace_back(cardPdr);
    t1.parseTerminusPDRs();

    auto card = t1.getEntity({PLDM_ENTITY_CARD, 1, 1});
    ASSERT_NE(nullptr, card);
    EXPECT_EQ(
        "/xyz/openbmc_project/inventory/system/Terminus_1_Container_1_Card_1",
        card->getPath());

    std::tuple<std::string, std::string, std::string> cardContainer{
        "contained_by", "containing",
        "/xyz/openbmc_project/inventory/system/Terminus_1"};
    EXPECT_EQ(cardContainer, card->getContainer());

    /* An entity contained by another entity is associated to the D-Bus
     * object of that entity
     */
    auto proc = t1.getEntity({PLDM_ENTITY_PROC, 1, 2});
    ASSERT_NE(nullptr, proc);

    std::tuple<std::string, std::string, std::string> procContainer{
        "contained_by", "containing",
        "/xyz/openbmc_project/inventory/system/Terminus_1_Container_1_Card_1"};
    EXPECT_EQ(procContainer, proc->getContainer());
}

TEST(TerminusTest, getPldmEntityNameTest)
{
    auto name = [](uint16_t entityType) {
        return pldm::dbus_api::getPldmEntityName(entityType).value_or("");
    };

    /* The entity type name identifies the entity type, so entity types which
     * share an Inventory.Item interface still have their own name
     */
    EXPECT_EQ("Cpu", name(PLDM_ENTITY_PROC));
    EXPECT_EQ("Gpu", name(PLDM_ENTITY_GPU));
    EXPECT_EQ("Accelerator", name(PLDM_ENTITY_ACCELERATOR));
    EXPECT_EQ("Board", name(PLDM_ENTITY_BOARD));
    EXPECT_EQ("SysBoard", name(PLDM_ENTITY_SYS_BOARD));
    EXPECT_EQ("Card", name(PLDM_ENTITY_CARD));

    /* An entity type with no Inventory.Item interface has no name */
    EXPECT_FALSE(pldm::dbus_api::getPldmEntityName(0xFFFF).has_value());

    /* An entity type has a name exactly when it has an Inventory.Item
     * interface
     */
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string basePath = "/xyz/openbmc_project/inventory/name_test/";
    for (uint32_t entityType = 0; entityType <= UINT16_MAX; entityType++)
    {
        auto entity = pldm::dbus_api::createPldmEntityForType(
            bus, basePath + std::to_string(entityType),
            static_cast<uint16_t>(entityType));
        EXPECT_EQ(pldm::dbus_api::getPldmEntityName(entityType).has_value(),
                  entity != nullptr)
            << "Failed for entity type " << entityType;
    }
}

TEST(TerminusTest, addPortConnectionAssociationTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);

    /* A network controller which contains two Ethernet ports */
    std::vector<uint8_t> networkControllerPdr{
        0x10, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x16,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0xe8,
        0x3, // containerID = 1000
        0x0, // associationType physical
        0x90,
        0x0, // container entityType network controller
        0x1,
        0x0, // container entity instance number = 1
        0x64,
        0x0, // container entity container ID = 100
        2,   // numberOfChildren
        0x2c,
        0x1, // child entityType ethernet
        0x1,
        0x0, // child entity instance number = 1
        0xe8,
        0x3, // child entity container ID = 1000
        0x2c,
        0x1, // child entityType ethernet
        0x2,
        0x0, // child entity instance number = 2
        0xe8,
        0x3  // child entity container ID = 1000
    };

    /* An Ethernet port whose container is not an entity type which a port may
     * be connected to
     */
    std::vector<uint8_t> sysBoardPdr{
        0x11, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x10,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0xe9,
        0x3, // containerID = 1001
        0x0, // associationType physical
        0x40,
        0x0, // container entityType system board
        0x1,
        0x0, // container entity instance number = 1
        0x0,
        0x0, // container entity container ID = 0
        1,   // numberOfChildren
        0x2c,
        0x1, // child entityType ethernet
        0x1,
        0x0, // child entity instance number = 1
        0xe9,
        0x3  // child entity container ID = 1001
    };

    t1.pdrs.emplace_back(networkControllerPdr);
    t1.pdrs.emplace_back(sysBoardPdr);
    t1.parseTerminusPDRs();

    const std::string pcieSwitchPath =
        "/xyz/openbmc_project/inventory/system/Terminus_1_Container_100_PCIeSwitch_1";

    std::tuple<std::string, std::string, std::string> containment{
        "contained_by", "containing", pcieSwitchPath};
    std::vector<std::tuple<std::string, std::string, std::string>> connection{
        {"connected_to", "connecting", pcieSwitchPath}};

    /* A port publishes both the containment and the connection association to
     * the network controller which contains it
     */
    auto firstPort = t1.getEntity({PLDM_ENTITY_ETHERNET, 1, 1000});
    ASSERT_NE(nullptr, firstPort);
    EXPECT_EQ(containment, firstPort->getContainer());
    EXPECT_EQ(connection, firstPort->getAssociations("connected_to"));

    auto secondPort = t1.getEntity({PLDM_ENTITY_ETHERNET, 2, 1000});
    ASSERT_NE(nullptr, secondPort);
    EXPECT_EQ(containment, secondPort->getContainer());
    EXPECT_EQ(connection, secondPort->getAssociations("connected_to"));

    /* The endpoint of the connection is not itself a port */
    auto pcieSwitch = t1.getEntity({PLDM_ENTITY_NETWORK_CONTROLLER, 1, 100});
    ASSERT_NE(nullptr, pcieSwitch);
    EXPECT_EQ(pcieSwitchPath, pcieSwitch->getPath());
    EXPECT_TRUE(pcieSwitch->getAssociations("connected_to").empty());

    /* A port whose container is not a connection endpoint publishes the
     * containment association only
     */
    auto unconnectedPort = t1.getEntity({PLDM_ENTITY_ETHERNET, 1, 1001});
    ASSERT_NE(nullptr, unconnectedPort);
    EXPECT_TRUE(unconnectedPort->getContainer().has_value());
    EXPECT_TRUE(unconnectedPort->getAssociations("connected_to").empty());
}

TEST(TerminusTest, addStateSensorTest)
{
    auto event = sdeventplus::Event::get_default();
    auto t1 = pldm::platform_mc::Terminus(
        1, 1 << PLDM_BASE | 1 << PLDM_PLATFORM, event);

    constexpr uint16_t unmappedEntityType = 0xFFFF;
    constexpr pldm::pdr::StateSetId healthStateSetId = 1;
    constexpr pldm::pdr::StateSetId unmappedStateSetId = 3;

    // Entity Auxiliary Names PDR: terminus name "S0"
    std::vector<uint8_t> entityAuxNamesPdr{
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
        0x0,  // Entity instance number = 1
        0,
        0,    // Overall system
        0,    // shared Name Count one name only
        01,   // nameStringCount
        0x65, 0x6e, 0x00,
        0x00, // Language Tag "en"
        0x53, 0x00, 0x30, 0x00,
        0x00  // Entity Name "S0"
    };

    // State Sensor PDR: sensorID = 1, on a power supply entity, with the
    // health state set at offset 0 and an unmapped state set at offset 1
    std::vector<uint8_t> stateSensorPdr{
        0x2, 0x0, 0x0,
        0x0,                   // record handle
        0x1,                   // PDRHeaderVersion
        PLDM_STATE_SENSOR_PDR, // PDRType
        0x0,
        0x0,                   // recordChangeNumber
        21,
        0,                     // dataLength
        /* State Sensor PDR Data*/
        0,
        0,            // PLDMTerminusHandle
        0x1,
        0x0,          // sensorID = 1
        PLDM_ENTITY_POWER_SUPPLY,
        0,            // entityType power supply
        1,
        0,            // entityInstanceNumber = 1
        0x1,
        0x0,          // containerID = 1
        PLDM_NO_INIT, // sensorInit
        false,        // sensorAuxiliaryNamesPDR
        2,            // compositeSensorCount
        healthStateSetId,
        0x0,          // stateSetID[0] health state
        0x1,          // possibleStatesSize[0]
        0x6,          // possibleStates[0] = {1,2}
        unmappedStateSetId,
        0x0,          // stateSetID[1] with no D-Bus interface
        0x1,          // possibleStatesSize[1]
        0x1e          // possibleStates[1] = {1,2,3,4}
    };

    // State Sensor PDR: sensorID = 2, on an entity type with no Inventory
    // Item interface, so no D-Bus object is published for its entity
    std::vector<uint8_t> unresolvedStateSensorPdr{
        0x3, 0x0, 0x0,
        0x0,                   // record handle
        0x1,                   // PDRHeaderVersion
        PLDM_STATE_SENSOR_PDR, // PDRType
        0x0,
        0x0,                   // recordChangeNumber
        17,
        0,                     // dataLength
        /* State Sensor PDR Data*/
        0,
        0,            // PLDMTerminusHandle
        0x2,
        0x0,          // sensorID = 2
        0xff,
        0xff,         // entityType with no Item interface
        1,
        0,            // entityInstanceNumber = 1
        0x1,
        0x0,          // containerID = 1
        PLDM_NO_INIT, // sensorInit
        false,        // sensorAuxiliaryNamesPDR
        1,            // compositeSensorCount
        healthStateSetId,
        0x0,          // stateSetID[0] health state
        0x1,          // possibleStatesSize[0]
        0x6           // possibleStates[0] = {1,2}
    };

    // Entity Association PDR: the terminus entity contains the power supply
    // and the entity type with no Inventory Item interface
    std::vector<uint8_t> entityAssociationPdr{
        0x4, 0x0, 0x0,
        0x0,                         // record handle
        0x1,                         // PDRHeaderVersion
        PLDM_PDR_ENTITY_ASSOCIATION, // PDRType
        0x1,
        0x0,                         // recordChangeNumber
        0x16,
        0,                           // dataLength
        /* Entity Association PDR Data*/
        0x1,
        0x0,  // containerID = 1
        0x0,  // associationType physical
        0x80,
        0x0,  // container entityType system software
        0x1,
        0x0,  // container entity instance number = 1
        0x0,
        0x0,  // container entity container ID = 0
        2,    // numberOfChildren
        PLDM_ENTITY_POWER_SUPPLY,
        0x0,  // child entityType power supply
        0x1,
        0x0,  // child entity instance number = 1
        0x1,
        0x0,  // child entity container ID = 1
        0xff,
        0xff, // child entityType with no Item interface
        0x1,
        0x0,  // child entity instance number = 1
        0x1,
        0x0   // child entity container ID = 1
    };

    t1.pdrs.emplace_back(entityAuxNamesPdr);
    t1.pdrs.emplace_back(stateSensorPdr);
    t1.pdrs.emplace_back(unresolvedStateSensorPdr);
    t1.pdrs.emplace_back(entityAssociationPdr);
    t1.parseTerminusPDRs();

    /* The State Sensor PDR resolves to the entity D-Bus object of the Entity
     * Association PDR
     */
    auto entity = t1.getEntity({PLDM_ENTITY_POWER_SUPPLY, 1, 1});
    ASSERT_NE(nullptr, entity);
    EXPECT_EQ(
        "/xyz/openbmc_project/inventory/system/Terminus_1_Container_1_PowerSupply_1",
        entity->getPath());

    /* The state sensor of an entity type with no Inventory Item interface has
     * no D-Bus object to publish on, so it is not constructed
     */
    EXPECT_EQ(nullptr, t1.getEntity({unmappedEntityType, 1, 1}));
    ASSERT_EQ(1, t1.stateSensors.size());
    EXPECT_EQ(nullptr, t1.getStateSensorObject(2));

    /* The state sensor of the resolved entity keeps every composite sensor
     * offset of its State Sensor PDR
     */
    auto stateSensor = t1.getStateSensorObject(1);
    ASSERT_NE(nullptr, stateSensor);
    EXPECT_EQ(1, stateSensor->getTid());
    EXPECT_EQ(2, stateSensor->getCompositeSensorCount());

    /* No state set has a D-Bus interface yet, so the entity D-Bus object
     * carries none and a present state publishes nothing
     */
    auto stateSets = entity->getStateSets();
    EXPECT_EQ(nullptr, stateSets->getStateSet(healthStateSetId));
    EXPECT_EQ(nullptr, stateSets->getStateSet(unmappedStateSetId));
    stateSensor->updatePresentState(0, 1);
}
