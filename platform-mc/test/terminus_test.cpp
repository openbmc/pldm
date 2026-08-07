#include "common/utils.hpp"
#include "platform-mc/dbus_impl_fru.hpp"
#include "platform-mc/terminus.hpp"

#include <libpldm/entity.h>

#include <sdbusplus/bus.hpp>

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

    // Verify property setters work through the abstract base
    auto entity = pldm::dbus_api::createPldmEntity(bus, basePath + "prop_test",
                                                   PLDM_ENTITY_PROC);
    ASSERT_NE(entity, nullptr);
    EXPECT_EQ("SN123", entity->serialNumber("SN123"));
    EXPECT_EQ("PN456", entity->partNumber("PN456"));
    EXPECT_EQ("TestMfg", entity->manufacturer("TestMfg"));
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
        0x1,  // associationType physical
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
        0x1,  // associationType physical
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

    t1.pdrs.emplace_back(entityAuxNamesPdr);
    t1.pdrs.emplace_back(procAuxNamesPdr);
    t1.pdrs.emplace_back(entityAssociationPdr);
    t1.pdrs.emplace_back(otherContainerPdr);
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
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/Terminus_1_Cpu_2_1",
              unnamedEntity->getPath());

    /* An entity instance number is unique within the container of the entity,
     * so two entities of the same type and instance number in two containers
     * are two objects
     */
    auto otherContainerEntity = t1.getEntity({PLDM_ENTITY_PROC, 2, 2});
    ASSERT_NE(nullptr, otherContainerEntity);
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/Terminus_1_Cpu_2_2",
              otherContainerEntity->getPath());

    /* An entity type with no Inventory Item interface is not exposed */
    EXPECT_EQ(nullptr, t1.getEntity({unmappedEntityType, 1, 1}));

    /* The container is the terminus inventory path */
    std::vector<std::tuple<std::string, std::string, std::string>> containers{
        {"contained_by", "containing",
         "/xyz/openbmc_project/inventory/system/S0"}};
    EXPECT_EQ(containers, namedEntity->getContainers());
    EXPECT_EQ(containers, unnamedEntity->getContainers());
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

    /* Every named entity type creates its Inventory.Item interface */
    auto& bus = pldm::utils::DBusHandler::getBus();
    std::string basePath = "/xyz/openbmc_project/inventory/name_test/";
    for (const auto& item : pldm::dbus_api::pldmEntityItems)
    {
        EXPECT_NE(nullptr,
                  pldm::dbus_api::createPldmEntityForType(
                      bus, basePath + std::string(item.name), item.entityType))
            << "Failed for " << item.name;
    }
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

    t1.pdrs.emplace_back(entityAuxNamesPdr);
    t1.pdrs.emplace_back(stateSensorPdr);
    t1.pdrs.emplace_back(unresolvedStateSensorPdr);
    t1.parseTerminusPDRs();

    /* The entity of a State Sensor PDR gets its D-Bus object */
    auto entity = t1.getEntity({PLDM_ENTITY_POWER_SUPPLY, 1, 1});
    ASSERT_NE(nullptr, entity);
    EXPECT_EQ(
        "/xyz/openbmc_project/inventory/system/Terminus_1_PowerSupply_1_1",
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
