#include "terminus.hpp"

#include "libpldm/platform.h"

#include "terminus_manager.hpp"

#include <ranges>

namespace pldm
{
namespace platform_mc
{

Terminus::Terminus(pldm_tid_t tid, uint64_t supportedTypes) :
    initialized(false), tid(tid), supportedTypes(supportedTypes)
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

void Terminus::parseTerminusPDRs()
{
    std::vector<std::shared_ptr<pldm_numeric_sensor_value_pdr>>
        numericSensorPdrs{};
    std::vector<std::shared_ptr<pldm_compact_numeric_sensor_pdr>>
        compactNumericSensorPdrs{};

    for (auto& pdr : pdrs)
    {
        auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(pdr.data());
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
            default:
            {
                lg2::error("Unsupported PDR with type {TYPE} handle {HANDLE}",
                           "TYPE", pdrHdr->type, "HANDLE",
                           static_cast<uint32_t>(pdrHdr->record_handle));
                break;
            }
        }
    }
}

std::shared_ptr<SensorAuxiliaryNames>
    Terminus::getSensorAuxiliaryNames(SensorId id)
{
    auto it = std::find_if(
        sensorAuxiliaryNamesTbl.begin(), sensorAuxiliaryNamesTbl.end(),
        [id](
            const std::shared_ptr<SensorAuxiliaryNames>& sensorAuxiliaryNames) {
        const auto& [sensorId, sensorCnt, sensorNames] = *sensorAuxiliaryNames;
        return sensorId == id;
    });

    if (it != sensorAuxiliaryNamesTbl.end())
    {
        return *it;
    }
    return nullptr;
};

std::shared_ptr<SensorAuxiliaryNames>
    Terminus::parseSensorAuxiliaryNamesPDR(const std::vector<uint8_t>& pdrData)
{
    constexpr uint8_t nullTerminator = 0;
    auto pdr = reinterpret_cast<const struct pldm_sensor_auxiliary_names_pdr*>(
        pdrData.data());
    const uint8_t* ptr = pdr->names;
    std::vector<std::vector<std::pair<NameLanguageTag, SensorName>>>
        sensorAuxNames{};
    char16_t alignedBuffer[PLDM_STR_UTF_16_MAX_LEN];
    for ([[maybe_unused]] const auto& sensor :
         std::views::iota(0, static_cast<int>(pdr->sensor_count)))
    {
        const uint8_t nameStringCount = static_cast<uint8_t>(*ptr);
        ptr += sizeof(uint8_t);
        std::vector<std::pair<NameLanguageTag, SensorName>> nameStrings{};
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
                lg2::error("Sensor name to long.");
                return nullptr;
            }
            memcpy(alignedBuffer, ptr, u16NameStringLen * sizeof(uint16_t));
            std::u16string u16NameString(alignedBuffer, u16NameStringLen);
            ptr += (u16NameString.size() + sizeof(nullTerminator)) *
                   sizeof(uint16_t);
            std::transform(u16NameString.cbegin(), u16NameString.cend(),
                           u16NameString.begin(),
                           [](uint16_t utf16) { return be16toh(utf16); });
            std::string nameString =
                std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,
                                     char16_t>{}
                    .to_bytes(u16NameString);
            std::replace(nameString.begin(), nameString.end(), ' ', '_');
            auto nullTerminatorPos = nameString.find('\0');
            if (nullTerminatorPos != std::string::npos)
            {
                nameString.erase(nullTerminatorPos);
            }
            nameStrings.emplace_back(
                std::make_pair(nameLanguageTag, nameString));
        }
        sensorAuxNames.emplace_back(std::move(nameStrings));
    }
    return std::make_shared<SensorAuxiliaryNames>(
        pdr->sensor_id, pdr->sensor_count, std::move(sensorAuxNames));
}

std::shared_ptr<pldm_numeric_sensor_value_pdr>
    Terminus::parseNumericSensorPDR(const std::vector<uint8_t>& pdr)
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

std::shared_ptr<SensorAuxiliaryNames>
    Terminus::parseCompactNumericSensorNames(const std::vector<uint8_t>& sPdr)
{
    std::vector<std::vector<std::pair<NameLanguageTag, SensorName>>>
        sensorAuxNames{};
    std::vector<std::pair<NameLanguageTag, SensorName>> nameStrings{};
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
    std::replace(nameString.begin(), nameString.end(), ' ', '_');
    auto nullTerminatorPos = nameString.find('\0');
    if (nullTerminatorPos != std::string::npos)
    {
        nameString.erase(nullTerminatorPos);
    }
    nameStrings.emplace_back(std::make_pair("en", nameString));
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

} // namespace platform_mc
} // namespace pldm
