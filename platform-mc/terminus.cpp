#include "terminus.hpp"

#include "libpldm/platform.h"

#include "terminus_manager.hpp"

namespace pldm
{
namespace platform_mc
{

Terminus::Terminus(tid_t tid, uint64_t supportedTypes) :
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
    /** @brief A list of parsed numeric sensor PDRs */
    std::vector<std::shared_ptr<pldm_numeric_sensor_value_pdr>>
        numericSensorPdrs{};

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
            std::u16string u16NameString(reinterpret_cast<const char16_t*>(ptr),
                                         0, PLDM_STR_UTF_16_MAX_LEN);
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
    size_t count = (uint8_t*)(&parsedPdr->hysteresis.value_u8) -
                   (uint8_t*)(&parsedPdr->hdr);

    size_t expectedPDRSize = PLDM_PDR_NUMERIC_SENSOR_PDR_MIN_LENGTH;
    if (pdr.size() < expectedPDRSize)
    {
        std::cerr << "parseNumericSensorPDR() Corrupted PDR, size="
                  << pdr.size() << "\n";
        return nullptr;
    }

    memcpy(&parsedPdr->hdr, ptr, count);
    ptr += count;

    expectedPDRSize -= PLDM_PDR_NUMERIC_SENSOR_PDR_MIN_LENGTH;
    switch (parsedPdr->sensor_data_size)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            expectedPDRSize += 3 * sizeof(uint8_t);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            expectedPDRSize += 3 * sizeof(uint16_t);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            expectedPDRSize += 3 * sizeof(uint32_t);
            break;
        default:
            break;
    }

    if (pdr.size() < expectedPDRSize)
    {
        std::cerr << "parseNumericSensorPDR() Corrupted PDR, size="
                  << pdr.size() << "\n";
        return nullptr;
    }

    switch (parsedPdr->range_field_format)
    {
        case PLDM_RANGE_FIELD_FORMAT_UINT8:
        case PLDM_RANGE_FIELD_FORMAT_SINT8:
            expectedPDRSize += 9 * sizeof(uint8_t);
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT16:
        case PLDM_RANGE_FIELD_FORMAT_SINT16:
            expectedPDRSize += 9 * sizeof(uint16_t);
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT32:
        case PLDM_RANGE_FIELD_FORMAT_SINT32:
        case PLDM_RANGE_FIELD_FORMAT_REAL32:
            expectedPDRSize += 9 * sizeof(uint32_t);
            break;
        default:
            break;
    }

    if (pdr.size() < expectedPDRSize)
    {
        std::cerr << "parseNumericSensorPDR() Corrupted PDR, size="
                  << pdr.size() << "\n";
        return nullptr;
    }

    switch (parsedPdr->sensor_data_size)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            parsedPdr->hysteresis.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->hysteresis.value_u8);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            parsedPdr->hysteresis.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->hysteresis.value_u16);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            parsedPdr->hysteresis.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->hysteresis.value_u32);
            break;
        default:
            break;
    }

    count = (uint8_t*)&parsedPdr->max_readable.value_u8 -
            (uint8_t*)&parsedPdr->supported_thresholds;
    memcpy(&parsedPdr->supported_thresholds, ptr, count);
    ptr += count;

    switch (parsedPdr->sensor_data_size)
    {
        case PLDM_SENSOR_DATA_SIZE_UINT8:
        case PLDM_SENSOR_DATA_SIZE_SINT8:
            parsedPdr->max_readable.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->max_readable.value_u8);
            parsedPdr->min_readable.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->min_readable.value_u8);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT16:
        case PLDM_SENSOR_DATA_SIZE_SINT16:
            parsedPdr->max_readable.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->max_readable.value_u16);
            parsedPdr->min_readable.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->min_readable.value_u16);
            break;
        case PLDM_SENSOR_DATA_SIZE_UINT32:
        case PLDM_SENSOR_DATA_SIZE_SINT32:
            parsedPdr->max_readable.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->max_readable.value_u32);
            parsedPdr->min_readable.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->min_readable.value_u32);
            break;
        default:
            break;
    }

    count = (uint8_t*)&parsedPdr->nominal_value.value_u8 -
            (uint8_t*)&parsedPdr->range_field_format;
    memcpy(&parsedPdr->range_field_format, ptr, count);
    ptr += count;

    switch (parsedPdr->range_field_format)
    {
        case PLDM_RANGE_FIELD_FORMAT_UINT8:
        case PLDM_RANGE_FIELD_FORMAT_SINT8:
            parsedPdr->nominal_value.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->nominal_value.value_u8);
            parsedPdr->normal_max.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->normal_max.value_u8);
            parsedPdr->normal_min.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->normal_min.value_u8);
            parsedPdr->warning_high.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->warning_high.value_u8);
            parsedPdr->warning_low.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->warning_low.value_u8);
            parsedPdr->critical_high.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->critical_high.value_u8);
            parsedPdr->critical_low.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->critical_low.value_u8);
            parsedPdr->fatal_high.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->fatal_high.value_u8);
            parsedPdr->fatal_low.value_u8 = *((uint8_t*)ptr);
            ptr += sizeof(parsedPdr->fatal_low.value_u8);
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT16:
        case PLDM_RANGE_FIELD_FORMAT_SINT16:
            parsedPdr->nominal_value.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->nominal_value.value_u16);
            parsedPdr->normal_max.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->normal_max.value_u16);
            parsedPdr->normal_min.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->normal_min.value_u16);
            parsedPdr->warning_high.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->warning_high.value_u16);
            parsedPdr->warning_low.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->warning_low.value_u16);
            parsedPdr->critical_high.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->critical_high.value_u16);
            parsedPdr->critical_low.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->critical_low.value_u16);
            parsedPdr->fatal_high.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->fatal_high.value_u16);
            parsedPdr->fatal_low.value_u16 = le16toh(*((uint16_t*)ptr));
            ptr += sizeof(parsedPdr->fatal_low.value_u16);
            break;
        case PLDM_RANGE_FIELD_FORMAT_UINT32:
        case PLDM_RANGE_FIELD_FORMAT_SINT32:
        case PLDM_RANGE_FIELD_FORMAT_REAL32:
            parsedPdr->nominal_value.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->nominal_value.value_u32);
            parsedPdr->normal_max.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->normal_max.value_u32);
            parsedPdr->normal_min.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->normal_min.value_u32);
            parsedPdr->warning_high.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->warning_high.value_u32);
            parsedPdr->warning_low.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->warning_low.value_u32);
            parsedPdr->critical_high.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->critical_high.value_u32);
            parsedPdr->critical_low.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->critical_low.value_u32);
            parsedPdr->fatal_high.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->fatal_high.value_u32);
            parsedPdr->fatal_low.value_u32 = le32toh(*((uint32_t*)ptr));
            ptr += sizeof(parsedPdr->fatal_low.value_u32);
            break;
        default:
            break;
    }
    return parsedPdr;
}

void Terminus::addNumericSensor(
    const std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr)
{
    uint16_t sensorId = pdr->sensor_id;
    std::string sensorName =
        "PLDM_Device_" + std::to_string(sensorId) + "_" + std::to_string(tid);

    if (pdr->sensor_auxiliary_names_pdr)
    {
        auto sensorAuxiliaryNames = getSensorAuxiliaryNames(sensorId);
        if (sensorAuxiliaryNames)
        {
            const auto& [sensorId, sensorCnt, sensorNames] =
                *sensorAuxiliaryNames;
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
        auto pdrData = *pdr;
        auto ptData = reinterpret_cast<uint8_t*>(&pdrData);
        auto vPdrData = std::vector<uint8_t>(ptData, ptData + sizeof(pdrData));
        std::shared_ptr<SensorIntf> sensor = std::make_shared<NumericSensor>(
            tid, true, vPdrData, sensorId, sensorName, inventoryPath);
        sensorObjects.emplace_back(sensor);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to create NumericSensor. ERROR=" << e.what()
                  << "sensorName=" << sensorName << "\n";
    }
}

} // namespace platform_mc
} // namespace pldm
