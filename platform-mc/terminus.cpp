#include "terminus.hpp"

#include "common/start_lifetime_as.hpp"
#include "dbus_impl_fru.hpp"

#include <libpldm/platform.h>
#include <libpldm/state_set.h>

#include <common/utils.hpp>

#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>

namespace pldm
{
namespace platform_mc
{

Terminus::Terminus(pldm_tid_t tid, uint64_t supportedTypes,
                   sdeventplus::Event& event) :
    initialized(false), maxBufferSize(PLDM_PLATFORM_EVENT_MSG_MAX_BUFFER_SIZE),
    synchronyConfigurationSupported(0), pollEvent(false), tid(tid),
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

uint16_t Terminus::findTerminusEntityType()
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
        return key.type;
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

    inventoryPath = "/xyz/openbmc_project/inventory/system/board/" + tName;
    try
    {
        inventoryItemInft = pldm::dbus_api::createPldmEntity(
            utils::DBusHandler::getBus(), inventoryPath, entityType);
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

    addNextSensorFromPDRs();
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
    else if (pdrIt <
             numericSensorPdrs.size() + compactNumericSensorPdrs.size() +
                 stateSensorPdrs.size())
    {
        pdrIt -= numericSensorPdrs.size() + compactNumericSensorPdrs.size();
        const auto& pdr = stateSensorPdrs[pdrIt];
        // Defer adding the next State Sensor
        sensorCreationEvent = std::make_unique<sdeventplus::source::Defer>(
            event,
            std::bind(std::mem_fn(&Terminus::addStateSensor), this, pdr));
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
    }

    auto sensorId = pdr->sensor_id;
    auto sensorNames = getSensorNames(sensorId);

    if (sensorNames.empty())
    {
        lg2::error(
            "Terminus ID {TID}: Failed to get name for Numeric Sensor {SID}",
            "TID", tid, "SID", sensorId);
        addNextSensorFromPDRs();
    }

    std::string sensorName = sensorNames.front();

    try
    {
        auto sensor = std::make_shared<NumericSensor>(
            tid, true, pdr, sensorName, inventoryPath);
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
    }

    auto sensorId = pdr->sensor_id;
    auto sensorNames = getSensorNames(sensorId);

    if (sensorNames.empty())
    {
        lg2::error(
            "Terminus ID {TID}: Failed to get name for Compact Numeric Sensor {SID}",
            "TID", tid, "SID", sensorId);
        addNextSensorFromPDRs();
    }

    std::string sensorName = sensorNames.front();

    try
    {
        auto sensor = std::make_shared<NumericSensor>(
            tid, true, pdr, sensorName, inventoryPath);
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

/* Minimal DSP0249 state set ID -> snake_case name table. The name is both
 * the `/xyz/openbmc_project/sensors/<name>/` namespace element and the key
 * to the Sensor.State.<StateSetName> interface a later commit hangs on the
 * object. A state set absent from this table (e.g. an OEM state set with no
 * phosphor-dbus-interfaces interface) gets no object. Extend as state sets
 * gain their interfaces; the full StateSet framework subsumes this table. */
static std::optional<std::string> getStateSetName(StateSetId id)
{
    static const std::map<StateSetId, std::string> names = {
        {PLDM_STATE_SET_HEALTH_STATE, "health"},
    };
    auto it = names.find(id);
    if (it == names.end())
    {
        return std::nullopt;
    }
    return it->second;
}

std::shared_ptr<StateSensorInfo> Terminus::parseStateSensorPDR(
    const std::vector<uint8_t>& pdrData)
{
    if (pdrData.size() < sizeof(pldm_state_sensor_pdr))
    {
        return nullptr;
    }

    auto info = std::make_shared<StateSensorInfo>();
    auto pdr =
        std::start_lifetime_as<const pldm_state_sensor_pdr>(pdrData.data());
    info->pdr.sensor_id = pdr->sensor_id;
    info->pdr.entity_type = pdr->entity_type;
    info->pdr.entity_instance_number = pdr->entity_instance;
    info->pdr.container_id = pdr->container_id;
    info->pdr.composite_sensor_count = pdr->composite_sensor_count;

    /* Walk the possible_states[] region one composite sensor at a time,
     * bounding every read against the end of the PDR buffer. Each composite
     * entry is a state_sensor_possible_states: a 3-byte header (state set ID
     * plus possible states size) followed by that many state bitfield bytes. */
    const uint8_t* statesPtr = pdr->possible_states;
    const uint8_t* pdrEnd = pdrData.data() + pdrData.size();
    constexpr size_t possibleStatesHdrSize =
        sizeof(state_sensor_possible_states::state_set_id) +
        sizeof(state_sensor_possible_states::possible_states_size);
    for (uint8_t offset = 0; offset < pdr->composite_sensor_count; offset++)
    {
        if (statesPtr + possibleStatesHdrSize > pdrEnd)
        {
            return nullptr;
        }
        auto possibleStates =
            std::start_lifetime_as<const state_sensor_possible_states>(
                statesPtr);
        const auto setId = possibleStates->state_set_id;
        const uint8_t possibleStatesSize = possibleStates->possible_states_size;
        const uint8_t* stateBytes = statesPtr + possibleStatesHdrSize;
        if (stateBytes + possibleStatesSize > pdrEnd)
        {
            return nullptr;
        }

        std::set<uint8_t> possibleStateValues{};
        for (uint8_t byteIndex = 0; byteIndex < possibleStatesSize; byteIndex++)
        {
            const uint8_t stateBits = stateBytes[byteIndex];
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                if (stateBits & (1 << bit))
                {
                    possibleStateValues.insert(byteIndex * 8 + bit);
                }
            }
        }
        info->compositeInfo.emplace_back(setId, std::move(possibleStateValues));

        statesPtr = stateBytes + possibleStatesSize;
    }

    return info;
}

void Terminus::addStateSensor(const std::shared_ptr<StateSensorInfo> info)
{
    if (!info)
    {
        lg2::error(
            "Terminus ID {TID}: Skip adding State Sensor - invalid pointer to PDR.",
            "TID", tid);
        addNextSensorFromPDRs();
        return;
    }

    auto sensorId = info->pdr.sensor_id;
    auto sensorNames = getSensorNames(sensorId);

    if (sensorNames.empty())
    {
        lg2::error(
            "Terminus ID {TID}: Failed to get name for State Sensor {SID}",
            "TID", tid, "SID", sensorId);
        addNextSensorFromPDRs();
        return;
    }

    const uint8_t offsetCount = info->compositeInfo.size();

    /* One D-Bus object per component sensor (composite sensor offset). The
     * sensor's aux name (or its default Sensor_<ID> name) is shared by all
     * offsets, so the component index is appended to keep each object unique
     * within the terminus - covering a composite sensor that repeats a state
     * set on more than one offset. A component sensor whose state set has no
     * snake_case name (an OEM or otherwise unmapped state set) gets no
     * object. */
    const auto& baseName = sensorNames.front();
    for (uint8_t offset = 0; offset < offsetCount; offset++)
    {
        const StateSetId stateSetId = info->compositeInfo[offset].first;
        auto stateSetName = getStateSetName(stateSetId);
        if (!stateSetName)
        {
            lg2::info(
                "Terminus ID {TID} sensor {SID} offset {OFF}: state set {SS} has no interface, skipping.",
                "TID", tid, "SID", sensorId, "OFF", offset, "SS", stateSetId);
            continue;
        }

        auto label = std::format("{}_Component_{}", baseName, unsigned(offset));
        try
        {
            auto sensor = std::make_shared<StateSensor>(
                tid, true, info, offset, *stateSetName, label, inventoryPath);
            lg2::info("Created StateSensor {NAME}", "NAME", label);
            stateSensors.emplace_back(sensor);
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to create StateSensor. error - {ERROR} label - {NAME}",
                "ERROR", e, "NAME", label);
        }
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
            lg2::error(
                "Does not support Fru Record ID Type {TYPE} of terminus {TID}",
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
                    inventoryItemInft->model(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_PN:
                    inventoryItemInft->partNumber(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_SN:
                    inventoryItemInft->serialNumber(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_MANUFAC:
                    inventoryItemInft->manufacturer(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_NAME:
                    inventoryItemInft->names({fruField});
                    break;
                case PLDM_FRU_FIELD_TYPE_VERSION:
                    inventoryItemInft->version(fruField);
                    break;
                case PLDM_FRU_FIELD_TYPE_ASSET_TAG:
                    inventoryItemInft->assetTag(fruField);
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
