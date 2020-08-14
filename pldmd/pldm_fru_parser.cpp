#include "pldmd/pldm_fru_parser.hpp"

#include "common/utils.hpp"

#include <iostream>
#include <variant>

namespace pldm
{

namespace fruparser
{

using Value =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string, std::vector<uint8_t>>;

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

        if (startOffset != recordStartOffset)
        {
            objectPath.append(std::to_string(fruRecordData->record_set_id));
            pldmfruObjs.push_back(
                std::make_unique<PLDMFRUParser<T>>(bus, objectPath));
        }

        tlvOffset = 0;
        for (auto fieldNum = 0; fieldNum < fruRecordData->num_fru_fields;
             fieldNum++)
        {
            fruRecordTlv =
                (struct pldm_fru_record_tlv*)(&fruRecordData->tlvs->type +
                                              tlvOffset);
            uint8_t* valuePtr = fruRecordTlv->value;

            if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_CHASSIS)
            {
                this->dBusMap.propertyName = "Chassis";
                this->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_MODEL)
            {
                std::cerr << "reached here " << std::endl;
                this->dBusMap.propertyName = "Model";
                this->dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_PN)
            {
                dBusMap.propertyName = "PN";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_SN)
            {
                dBusMap.propertyName = "SN";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_MANUFAC)
            {
                dBusMap.propertyName = "Manufacturer";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_MANUFAC_DATE)
            {
                dBusMap.propertyName = "ManufacturerDate";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_VENDOR)
            {
                dBusMap.propertyName = "Vendor";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_NAME)
            {
                dBusMap.propertyName = "Name";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_SKU)
            {
                dBusMap.propertyName = "SKU";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_VERSION)
            {
                dBusMap.propertyName = "Version";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_ASSET_TAG)
            {
                dBusMap.propertyName = "AssetTag";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_DESC)
            {
                dBusMap.propertyName = "Description";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_EC_LVL)
            {
                dBusMap.propertyName = "ECLevel";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_OTHER)
            {
                dBusMap.propertyName = "Other";
                dBusMap.propertyType = "string";
                std::string value(valuePtr, valuePtr + fruRecordTlv->length);
                dbusHandler->setDbusProperty(dBusMap, PropertyValue{value});
            }
            else if (fruRecordTlv->type == PLDM_FRU_FIELD_TYPE_IANA)
            {
                dBusMap.propertyName = "IANA";
                dBusMap.propertyType = "uint32";
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

} // namespace fruparser
} // namespace pldm
