#include "terminus.hpp"

#include "common/start_lifetime_as.hpp"
#include "dbus_impl_fru.hpp"

#include <libpldm/platform.h>

#include <common/utils.hpp>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <ranges>

namespace pldm
{
namespace platform_mc
{

/** @brief The base of the inventory D-Bus object paths of a terminus */
constexpr auto inventoryPathBase = "/xyz/openbmc_project/inventory/system/";

Terminus::Terminus(pldm_tid_t tid, uint64_t supportedTypes,
                   sdeventplus::Event& event) :
    initialized(false), maxBufferSize(PLDM_PLATFORM_EVENT_MSG_MAX_BUFFER_SIZE),
    synchronyConfigurationSupported(0), tid(tid),
    supportedTypes(supportedTypes), event(event)
{}

bool Terminus::doesSupportType(uint8_t type)
{
    return supportedTypes.test(type);
}

bool Terminus::doesSupportCommand(uint8_t type, uint8_t command)
{
    if (!doesSupportType(type))
    {
        return false;
    }

    try
    {
        const size_t idx = type * (PLDM_MAX_CMDS_PER_TYPE / 8) + (command / 8);
        if (idx >= supportedCmds.size())
        {
            return false;
        }

        if (supportedCmds[idx] & (1 << (command % 8)))
        {
            lg2::info(
                "PLDM type {TYPE} command {CMD} is supported by terminus {TID}",
                "TYPE", type, "CMD", command, "TID", getTid());
            return true;
        }
    }
    catch (const std::exception& e)
    {
        return false;
    }

    return false;
}

std::optional<std::string_view> Terminus::findTerminusName()
{
    auto it = std::find_if(
        entityAuxiliaryNamesTbl.begin(), entityAuxiliaryNamesTbl.end(),
        [](const std::shared_ptr<EntityAuxiliaryNames>& entityAuxiliaryNames) {
            const auto& [key, entityNames] = *entityAuxiliaryNames;
            /**
             * There is only one Overall system container entity in one
             * terminus. The entity auxiliary name PDR of that terminus with the
             * that type of containerID will include terminus name.
             */
            return (
                entityAuxiliaryNames &&
                key.containerId == PLDM_PLATFORM_ENTITY_SYSTEM_CONTAINER_ID &&
                entityNames.size());
        });

    if (it != entityAuxiliaryNamesTbl.end())
    {
        const auto& [key, entityNames] = **it;
        if (!entityNames.size())
        {
            return std::nullopt;
        }
        return entityNames[0].second;
    }

    return std::nullopt;
}

std::optional<EntityKey> Terminus::findTerminusEntityKey()
{
    auto it = std::find_if(
        entityAuxiliaryNamesTbl.begin(), entityAuxiliaryNamesTbl.end(),
        [](const std::shared_ptr<EntityAuxiliaryNames>& entityAuxiliaryNames) {
            const auto& [key, entityNames] = *entityAuxiliaryNames;
            return (
                entityAuxiliaryNames &&
                key.containerId == PLDM_PLATFORM_ENTITY_SYSTEM_CONTAINER_ID &&
                entityNames.size());
        });

    if (it != entityAuxiliaryNamesTbl.end())
    {
        const auto& [key, entityNames] = **it;
        return key;
    }

    /* Without an Entity Auxiliary Names PDR naming it, the overall terminus
     * entity is the physical container which the system container holds.
     */
    auto record = std::ranges::find_if(
        entityAssociations, [](const EntityAssociation& association) {
            return association.associationType ==
                       PLDM_ENTITY_ASSOCIAION_PHYSICAL &&
                   association.containerEntity.containerId ==
                       PLDM_PLATFORM_ENTITY_SYSTEM_CONTAINER_ID;
        });
    if (record != entityAssociations.end())
    {
        return record->containerEntity;
    }

    return std::nullopt;
}

uint16_t Terminus::findTerminusEntityType()
{
    auto key = findTerminusEntityKey();
    if (key)
    {
        return key->type;
    }

    return 0;
}

bool Terminus::createInventoryPath(std::string tName, uint16_t entityType)
{
    if (tName.empty())
    {
        return false;
    }

    /* inventory object is created */
    if (inventoryItemInft)
    {
        return false;
    }

    inventoryPath = inventoryPathBase + tName;
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        inventoryItemInft =
            pldm::dbus_api::createPldmEntity(bus, inventoryPath, entityType);
        terminusItemIntf =
            std::make_unique<InventoryItemIntf>(bus, inventoryPath.c_str());
        terminusItemIntf->prettyName(tName);
        terminusItemIntf->present(true);
        terminusStateSets = std::make_shared<StateSets>(inventoryPath);
        return true;
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Inventory Board interface for device {PATH}",
            "PATH", inventoryPath);
    }

    return false;
}

void Terminus::parseTerminusPDRs()
{
    for (auto& pdr : pdrs)
    {
        auto pdrHdr = std::start_lifetime_as<pldm_pdr_hdr>(pdr.data());
        switch (pdrHdr->type)
        {
            case PLDM_SENSOR_AUXILIARY_NAMES_PDR:
            {
                auto sensorAuxNames = parseSensorAuxiliaryNamesPDR(pdr);
                if (!sensorAuxNames)
                {
                    lg2::error(
                        "Failed to parse PDR with type {TYPE} handle {HANDLE}",
                        "TYPE", pdrHdr->type, "HANDLE",
                        static_cast<uint32_t>(pdrHdr->record_handle));
                    continue;
                }
                sensorAuxiliaryNamesTbl.emplace_back(std::move(sensorAuxNames));
                break;
            }
            case PLDM_NUMERIC_SENSOR_PDR:
            {
                auto parsedPdr = parseNumericSensorPDR(pdr);
                if (!parsedPdr)
                {
                    lg2::error(
                        "Failed to parse PDR with type {TYPE} handle {HANDLE}",
                        "TYPE", pdrHdr->type, "HANDLE",
                        static_cast<uint32_t>(pdrHdr->record_handle));
                    continue;
                }
                numericSensorPdrs.emplace_back(std::move(parsedPdr));
                break;
            }
            case PLDM_COMPACT_NUMERIC_SENSOR_PDR:
            {
                auto parsedPdr = parseCompactNumericSensorPDR(pdr);
                if (!parsedPdr)
                {
                    lg2::error(
                        "Failed to parse PDR with type {TYPE} handle {HANDLE}",
                        "TYPE", pdrHdr->type, "HANDLE",
                        static_cast<uint32_t>(pdrHdr->record_handle));
                    continue;
                }
                auto sensorAuxNames = parseCompactNumericSensorNames(pdr);
                if (!sensorAuxNames)
                {
                    lg2::error(
                        "Failed to parse sensor name PDR with type {TYPE} handle {HANDLE}",
                        "TYPE", pdrHdr->type, "HANDLE",
                        static_cast<uint32_t>(pdrHdr->record_handle));
                    continue;
                }
                compactNumericSensorPdrs.emplace_back(std::move(parsedPdr));
                sensorAuxiliaryNamesTbl.emplace_back(std::move(sensorAuxNames));
                break;
            }
            case PLDM_STATE_SENSOR_PDR:
            {
                auto parsedPdr = parseStateSensorPDR(pdr);
                if (!parsedPdr)
                {
                    lg2::error(
                        "Failed to parse PDR with type {TYPE} handle {HANDLE}",
                        "TYPE", pdrHdr->type, "HANDLE",
                        static_cast<uint32_t>(pdrHdr->record_handle));
                    continue;
                }
                stateSensorPdrs.emplace_back(std::move(parsedPdr));
                break;
            }
            case PLDM_ENTITY_AUXILIARY_NAMES_PDR:
            {
                auto entityNames = parseEntityAuxiliaryNamesPDR(pdr);
                if (!entityNames)
                {
                    lg2::error(
                        "Failed to parse sensor name PDR with type {TYPE} handle {HANDLE}",
                        "TYPE", pdrHdr->type, "HANDLE",
                        static_cast<uint32_t>(pdrHdr->record_handle));
                    continue;
                }
                entityAuxiliaryNamesTbl.emplace_back(std::move(entityNames));
                break;
            }
            case PLDM_PDR_ENTITY_ASSOCIATION:
            {
                auto association = parseEntityAssociationPDR(pdr);
                if (!association)
                {
                    lg2::error(
                        "Failed to parse PDR with type {TYPE} handle {HANDLE}",
                        "TYPE", pdrHdr->type, "HANDLE",
                        static_cast<uint32_t>(pdrHdr->record_handle));
                    continue;
                }
                entityAssociations.emplace_back(std::move(*association));
                break;
            }
            default:
            {
                lg2::error("Unsupported PDR with type {TYPE} handle {HANDLE}",
                           "TYPE", pdrHdr->type, "HANDLE",
                           static_cast<uint32_t>(pdrHdr->record_handle));
                break;
            }
        }
    }

    auto tName = findTerminusName();
    if (tName && !tName.value().empty())
    {
        lg2::info("Terminus {TID} has Auxiliary Name {NAME}.", "TID", tid,
                  "NAME", tName.value());
        terminusName = static_cast<std::string>(tName.value());
    }

    if (terminusName.empty())
    {
        terminusName = std::format("Terminus_{}", tid);
    }

    if (createInventoryPath(terminusName, findTerminusEntityType()))
    {
        lg2::info("Terminus ID {TID}: Created Inventory path {PATH}.", "TID",
                  tid, "PATH", inventoryPath);
    }

    addEntities();

    addStateSensors();

    addNextSensorFromPDRs();
}

std::optional<EntityAssociation> Terminus::parseEntityAssociationPDR(
    const std::vector<uint8_t>& pdrData)
{
    if (pdrData.size() > UINT16_MAX ||
        pdrData.size() <
            sizeof(pldm_pdr_hdr) +
                offsetof(pldm_pdr_entity_association, association_type) +
                sizeof(uint8_t))
    {
        return std::nullopt;
    }

    size_t numEntities = 0;
    pldm_entity* extracted = nullptr;
    pldm_entity_association_pdr_extract(
        pdrData.data(), static_cast<uint16_t>(pdrData.size()), &numEntities,
        &extracted);
    if (!extracted)
    {
        return std::nullopt;
    }

    /* The container entity is the first extracted entity, the contained
     * entities follow it.
     */
    EntityAssociation association{
        pdrData[sizeof(pldm_pdr_hdr) +
                offsetof(pldm_pdr_entity_association, association_type)],
        EntityKey{extracted[0].entity_type, extracted[0].entity_instance_num,
                  extracted[0].entity_container_id},
        {}};
    for (const auto& idx : std::views::iota(1, static_cast<int>(numEntities)))
    {
        association.containedEntities.emplace_back(EntityKey{
            extracted[idx].entity_type, extracted[idx].entity_instance_num,
            extracted[idx].entity_container_id});
    }
    free(extracted);

    return association;
}

std::shared_ptr<StateSensorInfo> Terminus::parseStateSensorPDR(
    const std::vector<uint8_t>& pdrData)
{
    auto info = std::make_shared<StateSensorInfo>();
    int rc = decode_pldm_platform_state_sensor_pdr(pdrData.data(),
                                                   pdrData.size(), &info->pdr);
    if (rc)
    {
        lg2::error("Failed to decode State Sensor PDR, error - {ERROR}",
                   "ERROR", rc);
        return nullptr;
    }

    pldm_platform_state_sensor_possible_states states{};
    foreach_pldm_platform_state_sensor_pdr_possible_states(
        pdrData.data(), pdrData.size(), states, rc)
    {
        const StateSetId setId = states.state_set_id;
        PossibleStates possibleStateValues{};
        uint8_t byteIndex = 0;
        bitfield8_t bitfield{};
        foreach_pldm_platform_state_sensor_pdr_states(states, bitfield, rc)
        {
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                if (bitfield.byte & (1 << bit))
                {
                    possibleStateValues.insert(byteIndex * 8 + bit);
                }
            }
            byteIndex++;
        }
        if (rc)
        {
            lg2::error(
                "Failed to decode possible states of State Sensor PDR of sensorID {SENSORID}, error - {ERROR}",
                "SENSORID", info->pdr.sensor_id, "ERROR", rc);
            return nullptr;
        }

        info->componentInfo.emplace_back(setId, std::move(possibleStateValues));
    }
    if (rc)
    {
        lg2::error(
            "Failed to walk possible states of State Sensor PDR of sensorID {SENSORID}, error - {ERROR}",
            "SENSORID", info->pdr.sensor_id, "ERROR", rc);
        return nullptr;
    }

    return info;
}

std::optional<std::string> Terminus::findEntityName(const EntityKey& key)
{
    for (const auto& entityAuxiliaryNames : entityAuxiliaryNamesTbl)
    {
        if (!entityAuxiliaryNames)
        {
            continue;
        }

        const auto& [entityKey, entityNames] = *entityAuxiliaryNames;
        if (entityKey != key)
        {
            continue;
        }

        for (const auto& [languageTag, name] : entityNames)
        {
            if (languageTag == "en" && !name.empty())
            {
                return name;
            }
        }
    }

    return std::nullopt;
}

std::string Terminus::getEntityObjectName(const EntityKey& key,
                                          std::string_view entityTypeName)
{
    auto name = findEntityName(key);
    if (name)
    {
        std::vector<EntityKey> namedKeys{};
        for (const auto& entityAuxiliaryNames : entityAuxiliaryNamesTbl)
        {
            if (!entityAuxiliaryNames)
            {
                continue;
            }

            const auto& entityKey = std::get<EntityKey>(*entityAuxiliaryNames);
            if (findEntityName(entityKey) == name &&
                std::ranges::find(namedKeys, entityKey) == namedKeys.end())
            {
                namedKeys.emplace_back(entityKey);
            }
        }

        /* A name given to more than one entity does not identify one of
         * them.
         */
        if (namedKeys.size() == 1)
        {
            return std::format("{}_{}", terminusName, *name);
        }
    }

    /* The entity instance number is unique within the container of the
     * entity, not within the terminus, so the container ID is part of the
     * name.
     */
    return std::format("Terminus_{}_Container_{}_{}_{}", tid, key.containerId,
                       entityTypeName, key.instanceIdx);
}

void Terminus::addEntity(const EntityKey& key)
{
    auto terminusEntityKey = findTerminusEntityKey();
    if (terminusEntityKey && key == *terminusEntityKey)
    {
        /* The overall terminus entity is exposed by the terminus inventory
         * path.
         */
        return;
    }

    if (getEntity(key))
    {
        return;
    }

    auto entityTypeName = pldm::dbus_api::getPldmEntityName(key.type);
    if (!entityTypeName)
    {
        lg2::info(
            "Terminus ID {TID}: Skip Entity type {TYPE} instance {INSTANCE} - no Inventory Item interface.",
            "TID", tid, "TYPE", key.type, "INSTANCE", key.instanceIdx);
        return;
    }

    auto name = getEntityObjectName(key, *entityTypeName);
    auto path = inventoryPathBase + name;

    try
    {
        auto typeItemIntf = pldm::dbus_api::createPldmEntityForType(
            utils::DBusHandler::getBus(), path, key.type);
        if (!typeItemIntf)
        {
            lg2::info(
                "Terminus ID {TID}: Skip Entity type {TYPE} instance {INSTANCE} - no Inventory Item interface.",
                "TID", tid, "TYPE", key.type, "INSTANCE", key.instanceIdx);
            return;
        }
        entities.emplace_back(
            std::make_shared<Entity>(key, path, name, std::move(typeItemIntf)));
        lg2::info("Terminus ID {TID}: Created Entity path {PATH}.", "TID", tid,
                  "PATH", path);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Failed to create Entity {PATH} error - {ERROR}", "PATH",
                   path, "ERROR", e);
    }
}

void Terminus::addEntities()
{
    for (const auto& association : entityAssociations)
    {
        addEntity(association.containerEntity);
        for (const auto& contained : association.containedEntities)
        {
            addEntity(contained);
        }
    }

    addEntityAssociations();
}

void Terminus::addEntityAssociations()
{
    auto terminusEntityKey = findTerminusEntityKey();

    for (const auto& association : entityAssociations)
    {
        if (association.associationType != PLDM_ENTITY_ASSOCIAION_PHYSICAL)
        {
            continue;
        }

        std::string containerPath{};
        auto container = getEntity(association.containerEntity);
        if (container)
        {
            containerPath = container->getPath();
        }
        else if (terminusEntityKey &&
                 association.containerEntity == *terminusEntityKey)
        {
            /* The overall terminus entity is exposed by the terminus
             * inventory path.
             */
            containerPath = inventoryPath;
        }

        for (const auto& contained : association.containedEntities)
        {
            auto entity = getEntity(contained);
            if (!entity)
            {
                continue;
            }

            /* A container with no D-Bus object hides the entities it
             * contains.
             */
            if (containerPath.empty())
            {
                continue;
            }

            if (!entity->addContainer(containerPath))
            {
                lg2::error(
                    "Terminus ID {TID}: Entity {PATH} already has a container, skip container {CONTAINER}.",
                    "TID", tid, "PATH", entity->getPath(), "CONTAINER",
                    containerPath);
                continue;
            }

            /* A pair of entity types may publish a further association on
             * top of the containment, which is how the contained entity is
             * reached from its container.
             */
            if (container)
            {
                auto assoc = pldm::dbus_api::findPldmEntityAssociation(
                    contained.type, association.containerEntity.type);
                if (assoc)
                {
                    entity->addAssociation(std::string(assoc->forward),
                                           std::string(assoc->reverse),
                                           container->getPath());
                }
            }
        }
    }
}

std::shared_ptr<Entity> Terminus::getEntity(const EntityKey& key)
{
    auto it = std::find_if(entities.begin(), entities.end(),
                           [&key](const std::shared_ptr<Entity>& entity) {
                               return entity && entity->getKey() == key;
                           });

    if (it != entities.end())
    {
        return *it;
    }

    return nullptr;
}

std::string Terminus::getEntityInventoryPath(const EntityKey& key)
{
    auto entity = getEntity(key);
    if (entity)
    {
        return entity->getPath();
    }

    return inventoryPath;
}

std::shared_ptr<StateSets> Terminus::getEntityStateSets(const EntityKey& key)
{
    auto entity = getEntity(key);
    if (entity)
    {
        return entity->getStateSets();
    }

    /* The overall terminus entity is exposed by the terminus inventory path */
    auto terminusEntityKey = findTerminusEntityKey();
    if (terminusEntityKey && key == *terminusEntityKey)
    {
        return terminusStateSets;
    }

    return nullptr;
}

void Terminus::addStateSensors()
{
    for (const auto& info : stateSensorPdrs)
    {
        EntityKey key{info->pdr.entity_type, info->pdr.entity_instance_number,
                      info->pdr.container_id};

        auto stateSets = getEntityStateSets(key);
        if (!stateSets)
        {
            lg2::error(
                "Terminus ID {TID}: Skip state sensor {SENSORID} - no D-Bus object for Entity type {TYPE} instance {INSTANCE} container {CONTAINER}.",
                "TID", tid, "SENSORID", info->pdr.sensor_id, "TYPE", key.type,
                "INSTANCE", key.instanceIdx, "CONTAINER", key.containerId);
            continue;
        }

        stateSensors.emplace_back(
            std::make_shared<StateSensor>(tid, info, std::move(stateSets)));
    }
}

std::shared_ptr<StateSensor> Terminus::getStateSensorObject(SensorID id)
{
    auto it =
        std::find_if(stateSensors.begin(), stateSensors.end(),
                     [id](const std::shared_ptr<StateSensor>& stateSensor) {
                         return stateSensor && stateSensor->getSensorId() == id;
                     });

    if (it != stateSensors.end())
    {
        return *it;
    }

    return nullptr;
}

void Terminus::addNextSensorFromPDRs()
{
    sensorCreationEvent.reset();

    if (terminusName.empty())
    {
        lg2::error(
            "Terminus ID {TID}: DOES NOT have name. Skip Adding sensors.",
            "TID", tid);
        return;
    }

    auto pdrIt = sensorPdrIt;

    if (pdrIt < numericSensorPdrs.size())
    {
        const auto& pdr = numericSensorPdrs[pdrIt];
        // Defer adding the next Numeric Sensor
        sensorCreationEvent = std::make_unique<sdeventplus::source::Defer>(
            event,
            std::bind(std::mem_fn(&Terminus::addNumericSensor), this, pdr));
    }
    else if (pdrIt < numericSensorPdrs.size() + compactNumericSensorPdrs.size())
    {
        pdrIt -= numericSensorPdrs.size();
        const auto& pdr = compactNumericSensorPdrs[pdrIt];
        // Defer adding the next Compact Numeric Sensor
        sensorCreationEvent = std::make_unique<sdeventplus::source::Defer>(
            event, std::bind(std::mem_fn(&Terminus::addCompactNumericSensor),
                             this, pdr));
    }
    else
    {
        sensorPdrIt = 0;
        return;
    }

    // Move the iteration to the next sensor PDR
    sensorPdrIt++;
}

std::shared_ptr<SensorAuxiliaryNames> Terminus::getSensorAuxiliaryNames(
    SensorID id)
{
    auto it = std::find_if(
        sensorAuxiliaryNamesTbl.begin(), sensorAuxiliaryNamesTbl.end(),
        [id](
            const std::shared_ptr<SensorAuxiliaryNames>& sensorAuxiliaryNames) {
            const auto& [sensorId, sensorCnt, sensorNames] =
                *sensorAuxiliaryNames;
            return sensorId == id;
        });

    if (it != sensorAuxiliaryNamesTbl.end())
    {
        return *it;
    }
    return nullptr;
};

std::shared_ptr<SensorAuxiliaryNames> Terminus::parseSensorAuxiliaryNamesPDR(
    const std::vector<uint8_t>& pdrData)
{
    constexpr uint8_t nullTerminator = 0;
    auto pdr = reinterpret_cast<const struct pldm_sensor_auxiliary_names_pdr*>(
        pdrData.data());
    const uint8_t* ptr = pdr->names;
    std::vector<AuxiliaryNames> sensorAuxNames{};
    char16_t alignedBuffer[PLDM_STR_UTF_16_MAX_LEN];
    for ([[maybe_unused]] const auto& sensor :
         std::views::iota(0, static_cast<int>(pdr->sensor_count)))
    {
        const uint8_t nameStringCount = static_cast<uint8_t>(*ptr);
        ptr += sizeof(uint8_t);
        AuxiliaryNames nameStrings{};
        for ([[maybe_unused]] const auto& count :
             std::views::iota(0, static_cast<int>(nameStringCount)))
        {
            std::string_view nameLanguageTag(
                reinterpret_cast<const char*>(ptr));
            ptr += nameLanguageTag.size() + sizeof(nullTerminator);

            int u16NameStringLen = 0;
            for (int i = 0; ptr[i] != 0 || ptr[i + 1] != 0; i += 2)
            {
                u16NameStringLen++;
            }
            /* include terminator */
            u16NameStringLen++;

            std::fill(std::begin(alignedBuffer), std::end(alignedBuffer), 0);
            if (u16NameStringLen > PLDM_STR_UTF_16_MAX_LEN)
            {
                lg2::error("Sensor name too long.");
                return nullptr;
            }
            memcpy(alignedBuffer, ptr, u16NameStringLen * sizeof(uint16_t));
            std::u16string u16NameString(alignedBuffer, u16NameStringLen);
            ptr += u16NameString.size() * sizeof(uint16_t);
            std::transform(u16NameString.cbegin(), u16NameString.cend(),
                           u16NameString.begin(),
                           [](uint16_t utf16) { return be16toh(utf16); });
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            std::string nameString{};
            try
            {
                nameString =
                    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,
                                         char16_t>{}
                        .to_bytes(u16NameString);
            }
            catch (const std::range_error& e)
            {
                lg2::error(
                    "Exception while converting UTF-16 to UTF-8 for sensor auxiliary name: {ERROR}, Skipping this name.",
                    "ERROR", e.what());
                continue;
            }
#pragma GCC diagnostic pop
            nameStrings.emplace_back(std::make_pair(
                nameLanguageTag, pldm::utils::trimNameForDbus(nameString)));
        }
        sensorAuxNames.emplace_back(std::move(nameStrings));
    }
    return std::make_shared<SensorAuxiliaryNames>(
        pdr->sensor_id, pdr->sensor_count, std::move(sensorAuxNames));
}

std::shared_ptr<EntityAuxiliaryNames> Terminus::parseEntityAuxiliaryNamesPDR(
    const std::vector<uint8_t>& pdrData)
{
    auto names_offset = sizeof(struct pldm_pdr_hdr) +
                        PLDM_PDR_ENTITY_AUXILIARY_NAME_PDR_MIN_LENGTH;
    auto names_size = pdrData.size() - names_offset;

    size_t decodedPdrSize =
        sizeof(struct pldm_entity_auxiliary_names_pdr) + names_size;
    auto vPdr = std::vector<char>(decodedPdrSize);
    auto decodedPdr =
        std::start_lifetime_as<pldm_entity_auxiliary_names_pdr>(vPdr.data());

    auto rc = decode_entity_auxiliary_names_pdr(pdrData.data(), pdrData.size(),
                                                decodedPdr, decodedPdrSize);

    if (rc)
    {
        lg2::error(
            "Failed to decode Entity Auxiliary Name PDR data, error {RC}.",
            "RC", rc);
        return nullptr;
    }

    auto vNames =
        std::vector<pldm_entity_auxiliary_name>(decodedPdr->name_string_count);
    decodedPdr->names = vNames.data();

    rc = decode_pldm_entity_auxiliary_names_pdr_index(decodedPdr);
    if (rc)
    {
        lg2::error("Failed to decode Entity Auxiliary Name, error {RC}.", "RC",
                   rc);
        return nullptr;
    }

    AuxiliaryNames nameStrings{};
    for (const auto& count :
         std::views::iota(0, static_cast<int>(decodedPdr->name_string_count)))
    {
        std::string_view nameLanguageTag =
            static_cast<std::string_view>(decodedPdr->names[count].tag);
        const size_t u16NameStringLen =
            std::char_traits<char16_t>::length(decodedPdr->names[count].name);
        std::u16string u16NameString(decodedPdr->names[count].name,
                                     u16NameStringLen);
        std::transform(u16NameString.cbegin(), u16NameString.cend(),
                       u16NameString.begin(),
                       [](uint16_t utf16) { return be16toh(utf16); });
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        std::string nameString{};
        try
        {
            nameString = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,
                                              char16_t>{}
                             .to_bytes(u16NameString);
        }
        catch (const std::range_error& e)
        {
            lg2::error(
                "Exception while converting UTF-16 to UTF-8 for entity auxiliary name: {ERROR}, Skipping this name.",
                "ERROR", e.what());
            continue;
        }
#pragma GCC diagnostic pop
        nameStrings.emplace_back(std::make_pair(
            nameLanguageTag, pldm::utils::trimNameForDbus(nameString)));
    }

    EntityKey key{decodedPdr->container.entity_type,
                  decodedPdr->container.entity_instance_num,
                  decodedPdr->container.entity_container_id};

    return std::make_shared<EntityAuxiliaryNames>(key, nameStrings);
}

std::shared_ptr<pldm_numeric_sensor_value_pdr> Terminus::parseNumericSensorPDR(
    const std::vector<uint8_t>& pdr)
{
    const uint8_t* ptr = pdr.data();
    auto parsedPdr = std::make_shared<pldm_numeric_sensor_value_pdr>();
    auto rc = decode_numeric_sensor_pdr_data(ptr, pdr.size(), parsedPdr.get());
    if (rc)
    {
        return nullptr;
    }
    return parsedPdr;
}

void Terminus::addNumericSensor(
    const std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr)
{
    if (!pdr)
    {
        lg2::error(
            "Terminus ID {TID}: Skip adding Numeric Sensor - invalid pointer to PDR.",
            "TID", tid);
        addNextSensorFromPDRs();
        return;
    }

    auto sensorId = pdr->sensor_id;
    auto sensorNames = getSensorNames(sensorId);

    if (sensorNames.empty())
    {
        lg2::error(
            "Terminus ID {TID}: Failed to get name for Numeric Sensor {SID}",
            "TID", tid, "SID", sensorId);
        addNextSensorFromPDRs();
        return;
    }

    std::string sensorName = sensorNames.front();

    try
    {
        auto sensor = std::make_shared<NumericSensor>(
            tid, true, pdr, sensorName,
            getEntityInventoryPath({pdr->entity_type, pdr->entity_instance_num,
                                    pdr->container_id}));
        lg2::info("Created NumericSensor {NAME}", "NAME", sensorName);
        numericSensors.emplace_back(sensor);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create NumericSensor. error - {ERROR} sensorname - {NAME}",
            "ERROR", e, "NAME", sensorName);
    }

    addNextSensorFromPDRs();
}

std::shared_ptr<SensorAuxiliaryNames> Terminus::parseCompactNumericSensorNames(
    const std::vector<uint8_t>& sPdr)
{
    std::vector<std::vector<std::pair<NameLanguageTag, SensorName>>>
        sensorAuxNames{};
    AuxiliaryNames nameStrings{};

    auto pdr =
        reinterpret_cast<const pldm_compact_numeric_sensor_pdr*>(sPdr.data());

    if (sPdr.size() <
        (sizeof(pldm_compact_numeric_sensor_pdr) - sizeof(uint8_t)))
    {
        return nullptr;
    }

    if (!pdr->sensor_name_length ||
        (sPdr.size() < (sizeof(pldm_compact_numeric_sensor_pdr) -
                        sizeof(uint8_t) + pdr->sensor_name_length)))
    {
        return nullptr;
    }

    std::string nameString(reinterpret_cast<const char*>(pdr->sensor_name),
                           pdr->sensor_name_length);
    nameStrings.emplace_back(
        std::make_pair("en", pldm::utils::trimNameForDbus(nameString)));
    sensorAuxNames.emplace_back(std::move(nameStrings));

    return std::make_shared<SensorAuxiliaryNames>(pdr->sensor_id, 1,
                                                  std::move(sensorAuxNames));
}

std::shared_ptr<pldm_compact_numeric_sensor_pdr>
    Terminus::parseCompactNumericSensorPDR(const std::vector<uint8_t>& sPdr)
{
    auto pdr =
        reinterpret_cast<const pldm_compact_numeric_sensor_pdr*>(sPdr.data());
    if (sPdr.size() < sizeof(pldm_compact_numeric_sensor_pdr))
    {
        // Handle error: input data too small to contain valid pdr
        return nullptr;
    }
    auto parsedPdr = std::make_shared<pldm_compact_numeric_sensor_pdr>();

    parsedPdr->hdr = pdr->hdr;
    parsedPdr->terminus_handle = pdr->terminus_handle;
    parsedPdr->sensor_id = pdr->sensor_id;
    parsedPdr->entity_type = pdr->entity_type;
    parsedPdr->entity_instance = pdr->entity_instance;
    parsedPdr->container_id = pdr->container_id;
    parsedPdr->sensor_name_length = pdr->sensor_name_length;
    parsedPdr->base_unit = pdr->base_unit;
    parsedPdr->unit_modifier = pdr->unit_modifier;
    parsedPdr->occurrence_rate = pdr->occurrence_rate;
    parsedPdr->range_field_support = pdr->range_field_support;
    parsedPdr->warning_high = pdr->warning_high;
    parsedPdr->warning_low = pdr->warning_low;
    parsedPdr->critical_high = pdr->critical_high;
    parsedPdr->critical_low = pdr->critical_low;
    parsedPdr->fatal_high = pdr->fatal_high;
    parsedPdr->fatal_low = pdr->fatal_low;
    return parsedPdr;
}

void Terminus::addCompactNumericSensor(
    const std::shared_ptr<pldm_compact_numeric_sensor_pdr> pdr)
{
    if (!pdr)
    {
        lg2::error(
            "Terminus ID {TID}: Skip adding Compact Numeric Sensor - invalid pointer to PDR.",
            "TID", tid);
        addNextSensorFromPDRs();
        return;
    }

    auto sensorId = pdr->sensor_id;
    auto sensorNames = getSensorNames(sensorId);

    if (sensorNames.empty())
    {
        lg2::error(
            "Terminus ID {TID}: Failed to get name for Compact Numeric Sensor {SID}",
            "TID", tid, "SID", sensorId);
        addNextSensorFromPDRs();
        return;
    }

    std::string sensorName = sensorNames.front();

    try
    {
        auto sensor = std::make_shared<NumericSensor>(
            tid, true, pdr, sensorName,
            getEntityInventoryPath(
                {pdr->entity_type, pdr->entity_instance, pdr->container_id}));
        lg2::info("Created Compact NumericSensor {NAME}", "NAME", sensorName);
        numericSensors.emplace_back(sensor);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to create Compact NumericSensor. error - {ERROR} sensorname - {NAME}",
            "ERROR", e, "NAME", sensorName);
    }

    addNextSensorFromPDRs();
}

std::shared_ptr<NumericSensor> Terminus::getSensorObject(SensorID id)
{
    if (terminusName.empty())
    {
        lg2::error(
            "Terminus ID {TID}: DOES NOT have terminus name. No numeric sensor object.",
            "TID", tid);
        return nullptr;
    }
    if (!numericSensors.size())
    {
        lg2::error("Terminus ID {TID} name {NAME}: DOES NOT have sensor.",
                   "TID", tid, "NAME", terminusName);
        return nullptr;
    }

    for (auto& sensor : numericSensors)
    {
        if (!sensor)
        {
            continue;
        }

        if (sensor->sensorId == id)
        {
            return sensor;
        }
    }

    return nullptr;
}

/** @brief Check if a pointer is go through end of table
 *  @param[in] table - pointer to FRU record table
 *  @param[in] p - pointer to each record of FRU record table
 *  @param[in] tableSize - FRU table size
 */
static bool isTableEnd(const uint8_t* table, const uint8_t* p,
                       const size_t tableSize)
{
    auto offset = p - table;
    return (tableSize - offset) < sizeof(struct pldm_fru_record_data_format);
}

void Terminus::updateInventoryWithFru(const uint8_t* fruData,
                                      const size_t fruLen)
{
    auto tmp = getTerminusName();
    if (!tmp || tmp.value().empty())
    {
        lg2::error(
            "Terminus ID {TID}: Failed to update Inventory with Fru Data - error : Terminus name is empty.",
            "TID", tid);
        return;
    }

    if (createInventoryPath(static_cast<std::string>(tmp.value()),
                            findTerminusEntityType()))
    {
        lg2::info("Terminus ID {TID}: Created Inventory path.", "TID", tid);
    }

    if (!fruDecoratorsIntf)
    {
        fruDecoratorsIntf = std::make_unique<pldm::dbus_api::PldmFruDecorators>(
            utils::DBusHandler::getBus(), inventoryPath);
    }

    auto ptr = fruData;
    while (!isTableEnd(fruData, ptr, fruLen))
    {
        auto record = reinterpret_cast<const pldm_fru_record_data_format*>(ptr);
        ptr += sizeof(pldm_fru_record_data_format) -
               sizeof(pldm_fru_record_tlv);

        if (!record->num_fru_fields)
        {
            lg2::error(
                "Invalid number of fields {NUM} of Record ID Type {TYPE} of terminus {TID}",
                "NUM", record->num_fru_fields, "TYPE", record->record_type,
                "TID", tid);
            return;
        }

        if (record->record_type != PLDM_FRU_RECORD_TYPE_GENERAL)
        {
            lg2::info(
                "Skipping unsupported FRU Record type {TYPE} of terminus {TID}",
                "TYPE", record->record_type, "TID", tid);

            for ([[maybe_unused]] const auto& idx :
                 std::views::iota(0, static_cast<int>(record->num_fru_fields)))
            {
                auto tlv = reinterpret_cast<const pldm_fru_record_tlv*>(ptr);
                ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
            }
            continue;
        }
        /* FRU General record type */
        for ([[maybe_unused]] const auto& idx :
             std::views::iota(0, static_cast<int>(record->num_fru_fields)))
        {
            auto tlv = reinterpret_cast<const pldm_fru_record_tlv*>(ptr);
            std::string fruField{};
            if (tlv->type != PLDM_FRU_FIELD_TYPE_IANA)
            {
                auto strOptional =
                    pldm::utils::fruFieldValuestring(tlv->value, tlv->length);
                if (!strOptional)
                {
                    ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
                    continue;
                }
                fruField = strOptional.value();

                if (fruField.empty())
                {
                    ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
                    continue;
                }
            }

            switch (tlv->type)
            {
                case PLDM_FRU_FIELD_TYPE_MODEL:
                    fruDecoratorsIntf->model(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_PN:
                    fruDecoratorsIntf->partNumber(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_SN:
                    fruDecoratorsIntf->serialNumber(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_MANUFAC:
                    fruDecoratorsIntf->manufacturer(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_NAME:
                    fruDecoratorsIntf->names({fruField});
                    break;
                case PLDM_FRU_FIELD_TYPE_VERSION:
                    fruDecoratorsIntf->version(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_ASSET_TAG:
                    fruDecoratorsIntf->assetTag(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_VENDOR:
                case PLDM_FRU_FIELD_TYPE_CHASSIS:
                case PLDM_FRU_FIELD_TYPE_SKU:
                case PLDM_FRU_FIELD_TYPE_DESC:
                case PLDM_FRU_FIELD_TYPE_EC_LVL:
                case PLDM_FRU_FIELD_TYPE_OTHER:
                    break;
                case PLDM_FRU_FIELD_TYPE_IANA:
                    auto iana =
                        pldm::utils::fruFieldParserU32(tlv->value, tlv->length);
                    if (!iana)
                    {
                        ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
                        continue;
                    }
                    break;
            }
            ptr += sizeof(pldm_fru_record_tlv) - 1 + tlv->length;
        }
    }
}

std::vector<std::string> Terminus::getSensorNames(const SensorID& sensorId)
{
    std::vector<std::string> sensorNames;
    std::string defaultName =
        std::format("{}_Sensor_{}", terminusName, unsigned(sensorId));
    // To ensure there's always a default name at offset 0
    sensorNames.emplace_back(defaultName);

    auto sensorAuxiliaryNames = getSensorAuxiliaryNames(sensorId);
    if (!sensorAuxiliaryNames)
    {
        return sensorNames;
    }

    const auto& [id, sensorCount, nameMap] = *sensorAuxiliaryNames;
    for (const unsigned int& i :
         std::views::iota(0, static_cast<int>(sensorCount)))
    {
        auto sensorName = defaultName;
        if (i > 0)
        {
            // Sensor name at offset 0 will be the default name
            sensorName += "_" + std::to_string(i);
        }

        for (const auto& [languageTag, name] : nameMap[i])
        {
            if (languageTag == "en" && !name.empty())
            {
                sensorName = std::format("{}_{}", terminusName, name);
            }
        }

        if (i >= sensorNames.size())
        {
            sensorNames.emplace_back(sensorName);
        }
        else
        {
            sensorNames[i] = sensorName;
        }
    }

    return sensorNames;
}

} // namespace platform_mc
} // namespace pldm
