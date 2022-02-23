#include "terminus.hpp"

#include "libpldm/platform.h"

#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

Terminus::Terminus(pldm_tid_t tid, uint64_t supportedTypes) :
    initalized(false), tid(tid), supportedTypes(supportedTypes)
{
    inventoryPath = "/xyz/openbmc_project/inventory/Item/Board/PLDM_Device_" +
                    std::to_string(tid);
    inventoryItemBoardInft = std::make_unique<InventoryItemBoardIntf>(
        utils::DBusHandler::getBus(), inventoryPath.c_str());
}

bool Terminus::doesSupport(uint8_t type)
{
    return supportedTypes.test(type);
}

bool Terminus::parsePDRs()
{
    bool rc = true;
    std::vector<std::shared_ptr<pldm_numeric_sensor_value_pdr>>
        numericSensorPdrs{};
    std::vector<std::shared_ptr<pldm_compact_numeric_sensor_pdr>>
        compactNumericSensorPdrs{};

    for (auto& pdr : pdrs)
    {
        auto pdrHdr = reinterpret_cast<pldm_pdr_hdr*>(pdr.data());
        if (pdrHdr->type == PLDM_SENSOR_AUXILIARY_NAMES_PDR)
        {
            auto sensorAuxiliaryNames = parseSensorAuxiliaryNamesPDR(pdr);
            sensorAuxiliaryNamesTbl.emplace_back(
                std::move(sensorAuxiliaryNames));
        }
        else if (pdrHdr->type == PLDM_NUMERIC_SENSOR_PDR)
        {
            auto parsedPdr = parseNumericSensorPDR(pdr);
            if (parsedPdr != nullptr)
            {
                numericSensorPdrs.emplace_back(std::move(parsedPdr));
            }
        }
        else if (pdrHdr->type == PLDM_COMPACT_NUMERIC_SENSOR_PDR)
        {
            auto parsedPdr = parseCompactNumericSensorPDR(pdr);
            if (parsedPdr != nullptr)
            {
                compactNumericSensorPdrs.emplace_back(std::move(parsedPdr));
                auto sensorAuxiliaryNames = parseCompactNumericSensorNames(pdr);
                if (sensorAuxiliaryNames != nullptr)
                {
                    sensorAuxiliaryNamesTbl.emplace_back(
                        std::move(sensorAuxiliaryNames));
                }
            }
        }
        else
        {
            std::cerr << "parsePDRs() Unsupported PDR, type="
                      << static_cast<unsigned>(pdrHdr->type) << "\n";
            rc = false;
        }
    }

    for (auto pdr : numericSensorPdrs)
    {
        addNumericSensor(pdr);
    }

    for (auto pdr : compactNumericSensorPdrs)
    {
        addCompactNumericSensor(pdr);
    }

    return rc;
}

std::shared_ptr<SensorAuxiliaryNames>
    Terminus::getSensorAuxiliaryNames(SensorId id)
{
    for (auto sensorAuxiliaryNames : sensorAuxiliaryNamesTbl)
    {
        const auto& [sensorId, sensorCnt, sensorNames] = *sensorAuxiliaryNames;
        if (sensorId == id)
        {
            return sensorAuxiliaryNames;
        }
    }
    return nullptr;
}

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
    for (int i = 0; i < pdr->sensor_count; i++)
    {
        const uint8_t nameStringCount = static_cast<uint8_t>(*ptr);
        ptr += sizeof(uint8_t);
        std::vector<std::pair<NameLanguageTag, SensorName>> nameStrings{};
        for (int j = 0; j < nameStringCount; j++)
        {
            std::string nameLanguageTag(reinterpret_cast<const char*>(ptr), 0,
                                        PLDM_STR_UTF_8_MAX_LEN);
            ptr += nameLanguageTag.size() + sizeof(nullTerminator);

            int u16NameStringLen = 0;
            for (int i = 0; ptr[i] != 0 || ptr[i + 1] != 0; i += 2)
            {
                u16NameStringLen++;
            }
            memset(alignedBuffer, 0,
                   PLDM_STR_UTF_16_MAX_LEN * sizeof(uint16_t));
            memcpy(alignedBuffer, ptr, u16NameStringLen * sizeof(uint16_t));
            std::u16string u16NameString(alignedBuffer, 0,
                                         PLDM_STR_UTF_16_MAX_LEN);
            ptr += (u16NameString.size() + sizeof(nullTerminator)) *
                   sizeof(uint16_t);
            std::transform(u16NameString.cbegin(), u16NameString.cend(),
                           u16NameString.begin(),
                           [](uint16_t utf16) { return be16toh(utf16); });
            std::string nameString =
                std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,
                                     char16_t>{}
                    .to_bytes(u16NameString);
            nameStrings.emplace_back(
                std::make_pair(nameLanguageTag, nameString));
        }
        sensorAuxNames.emplace_back(nameStrings);
    }
    return std::make_shared<SensorAuxiliaryNames>(
        pdr->sensor_id, pdr->sensor_count, sensorAuxNames);
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

void Terminus::addNumericSensor(
    const std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr)
{
    uint16_t sensorId = pdr->sensor_id;
    std::string sensorName = "PLDM_Device_" + std::to_string(sensorId) + "_" +
                             std::to_string(tid);

    if (pdr->sensor_auxiliary_names_pdr)
    {
        auto sensorAuxiliaryNames = getSensorAuxiliaryNames(sensorId);
        if (sensorAuxiliaryNames)
        {
            const auto& [sensorId, sensorCnt,
                         sensorNames] = *sensorAuxiliaryNames;
            if (sensorCnt == 1)
            {
                for (const auto& [languageTag, name] : sensorNames[0])
                {
                    if (languageTag == "en")
                    {
                        sensorName = name + "_" + std::to_string(sensorId) +
                                     "_" + std::to_string(tid);
                    }
                }
            }
        }
    }

    try
    {
        auto sensor = std::make_shared<NumericSensor>(
            tid, true, pdr, sensorName, inventoryPath);
        numericSensors.emplace_back(sensor);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to create NumericSensor. ERROR=" << e.what()
                  << "sensorName=" << sensorName << "\n";
    }
}

std::shared_ptr<SensorAuxiliaryNames>
    Terminus::parseCompactNumericSensorNames(const std::vector<uint8_t>& sPdr)
{
    std::string nameString;
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

    if (pdr->sensor_name_length == 0)
    {
        return nullptr;
    }

    if (sPdr.size() < (sizeof(pldm_compact_numeric_sensor_pdr) -
                       sizeof(uint8_t) + pdr->sensor_name_length))
    {
        return nullptr;
    }

    std::string sTemp(reinterpret_cast<const char*>(pdr->sensor_name),
                      pdr->sensor_name_length);
    size_t pos = 0;
    while ((pos = sTemp.find(" ")) != std::string::npos)
    {
        sTemp.replace(pos, 1, "_");
    }
    nameString = sTemp;

    nameString.erase(nameString.find('\0'));
    nameStrings.emplace_back(std::make_pair("en", nameString));
    sensorAuxNames.emplace_back(nameStrings);

    return std::make_shared<SensorAuxiliaryNames>(pdr->sensor_id, 1,
                                                  sensorAuxNames);
}

std::shared_ptr<pldm_compact_numeric_sensor_pdr>
    Terminus::parseCompactNumericSensorPDR(const std::vector<uint8_t>& sPdr)
{
    std::string nameString;
    std::vector<std::pair<NameLanguageTag, SensorName>> nameStrings{};
    auto pdr =
        reinterpret_cast<const pldm_compact_numeric_sensor_pdr*>(sPdr.data());
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
    uint16_t sensorId = pdr->sensor_id;
    std::string sensorName = "PLDM_Device_" + std::to_string(sensorId) + "_" +
                             std::to_string(tid);

    auto sensorAuxiliaryNames = getSensorAuxiliaryNames(sensorId);
    if (sensorAuxiliaryNames)
    {
        const auto& [sensorId, sensorCnt, sensorNames] = *sensorAuxiliaryNames;
        if (sensorCnt == 1)
        {
            for (const auto& [languageTag, name] : sensorNames[0])
            {
                if (languageTag == "en")
                {
                    {
                        sensorName = name + "_" + std::to_string(sensorId) +
                                     "_" + std::to_string(tid);
                    }
                }
            }
        }
    }

    try
    {
        auto sensor = std::make_shared<NumericSensor>(
            tid, true, pdr, sensorName, inventoryPath);
        numericSensors.emplace_back(sensor);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to create NumericSensor. ERROR=" << e.what()
                  << "sensorName=" << sensorName << "\n";
    }
}

} // namespace platform_mc
} // namespace pldm
