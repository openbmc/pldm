#include "pldmd/pldm_fru_parser.hpp"

#include "common/utils.hpp"

#include <variant>

namespace pldm
{

namespace fruparser
{

using vecFruObjs = std::vector<std::unique_ptr<PLDMFRUParser<FruTableIntf>>>;
vecFruObjs pldmFruObjs;

template <typename T>
int PLDMFRUParser<T>::parseFRUTable(const uint8_t* fruTable,
                                    size_t fruTableLength,
                                    size_t recordStartOffset,
                                    DBusHandler* const dbusHandler)
{
    std::vector<uint8_t> fruTableData(fruTable, fruTable + fruTableLength);
    size_t recordDataSize = 0;
    int tlvOffset;
    struct pldm_fru_record_tlv* fruRecordTlv;
    std::string objPath = "/xyz/openbmc_project/pldm/fru_record_set/";
    std::string objectPath = objPath;
    size_t startOffset = recordStartOffset;

    auto& bus = dbusHandler->getBus();

    if (fruTableLength == 0 || fruTableLength <= recordStartOffset)
    {
        return PLDM_ERROR_INVALID_LENGTH;
    }

    while (startOffset < fruTableLength)
    {
        auto fruRecordData =
            reinterpret_cast<struct pldm_fru_record_data_format*>(
                fruTableData.data() + startOffset);
        auto rc = fru_record_data_size_calc(fruTable, startOffset,
                                            fruTableLength, &recordDataSize);
        if (rc != PLDM_SUCCESS)
        {
            return PLDM_ERROR;
        }

        objectPath.append(std::to_string(fruRecordData->record_set_id));

        tlvOffset = 0;
        for (auto fieldNum = 0; fieldNum < fruRecordData->num_fru_fields;
             fieldNum++)
        {
            fruRecordTlv =
                (struct pldm_fru_record_tlv*)(&fruRecordData->tlvs->type +
                                              tlvOffset);
            uint8_t* valuePtr = fruRecordTlv->value;

            pldmFruObjs.push_back(std::make_unique<PLDMFRUParser<T>>(
                bus, (objectPath + "/" + std::to_string(fieldNum + 1))));

            if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_CHASSIS)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "Chassis";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_MODEL)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "Model";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_PN)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "PN";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_SN)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "SN";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_MANUFAC)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "Manufacturer";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_MANUFAC_DATE)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "ManufacturerDate";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_VENDOR)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "Vendor";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_NAME)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "Name";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_SKU)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "SKU";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_VERSION)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "Version";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_ASSET_TAG)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "AssetTag";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_DESC)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "Description";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_EC_LVL)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "ECLevel";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_OTHER)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "Other";
                pldmFruObjs.back()->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_IANA)
            {
                pldmFruObjs.back()->dBusMap.propertyName = "IANA";
                pldmFruObjs.back()->dBusMap.propertyType = "uint32";
                uint8_t value = *valuePtr;
                dbusHandler->setDbusProperty(dBusMap,
                                             PropertyValue{(uint32_t)value});
            }

            tlvOffset += fruRecordTlv->length + sizeof(fruRecordTlv->type) +
                         sizeof(fruRecordTlv->length);
        }
        startOffset += recordDataSize;
        objectPath = objPath;
    }

    return PLDM_SUCCESS;
}

template <typename T>
std::vector<std::unique_ptr<PLDMFRUParser<T>>>& PLDMFRUParser<T>::getvecObjs()
{
    return pldmFruObjs;
}

} // namespace fruparser
} // namespace pldm
