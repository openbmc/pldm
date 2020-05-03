#include "pldm_fru_parser.hpp"

namespace pldm
{
FruProps PLDMFRUParser::fruProps(const uint8_t* fruRecord)
{
    FruProps fruPropsData;
    FruProperties fruProperties;
    auto fruRecordData =
        reinterpret_cast<const pldm_fru_record_data_format*>(fruRecord);

    if (fruRecordData->record_type == PLDM_FRU_RECORD_TYPE_GENERAL)
    {
        if (fruRecordData->encoding_type == PLDM_FRU_ENCODING_ASCII)
        {
            auto fruRecordDataPtr = &fruRecordData->tlvs[0];
            parseFruFeildData(fruRecordDataPtr, fruRecordData->num_fru_fields,
                              &fruProperties);
        }
    }
    fruPropsData[assetInterface] = fruProperties;
    return fruPropsData;
}

void PLDMFRUParser::parseFruFeildData(
    const pldm_fru_record_tlv* fruRecordDataPtr, uint8_t fruFeildNumber,
    FruProperties* fruProperties)
{
    auto dataOffsetptr = 0;
    while (fruFeildNumber)
    {
        auto dataPtr = reinterpret_cast<const pldm_fru_record_tlv*>(
            &fruRecordDataPtr[dataOffsetptr]);
        if (dataPtr->type == PLDM_FRU_FIELD_TYPE_MODEL)
        {
            // Model in string
            std::string model(reinterpret_cast<const char*>(dataPtr->value),
                              dataPtr->length);
            fruProperties->emplace("Model", model);
        }
        else if (dataPtr->type == PLDM_FRU_FIELD_TYPE_PN)
        {
            // Part Number in string
            std::string partNumber(
                reinterpret_cast<const char*>(dataPtr->value), dataPtr->length);
            fruProperties->emplace("PartNumber", partNumber);
        }
        else if (dataPtr->type == PLDM_FRU_FIELD_TYPE_SN)
        {
            // Serial Number in string
            std::string serialNumber(
                reinterpret_cast<const char*>(dataPtr->value), dataPtr->length);
            fruProperties->emplace("SerialNumber", serialNumber);
        }
        else if (dataPtr->type == PLDM_FRU_FIELD_TYPE_MANUFAC)
        {
            // Manufacturer in string
            std::string manufacturer(
                reinterpret_cast<const char*>(dataPtr->value), dataPtr->length);
            fruProperties->emplace("Manufacturer", manufacturer);
        }
        else if (dataPtr->type == PLDM_FRU_FIELD_TYPE_MANUFAC_DATE)
        {
            // The date of item manufacture in YYYYMMDD format
            const auto ts =
                reinterpret_cast<const timestamp104_t*>(dataPtr->value);
            fruProperties->emplace("BuildDate",
                                   pldm::utils::timestamp104DateToString(ts));
        }
        dataOffsetptr +=
            sizeof(dataPtr->type) + sizeof(dataPtr->length) + dataPtr->length;
        fruFeildNumber--;
    }
    return;
} // namespace pldm

} // namespace pldm