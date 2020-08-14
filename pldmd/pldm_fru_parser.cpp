#include "pldmd/pldm_fru_parser.hpp"

namespace pldm
{

namespace fruparser
{

template <typename T>
int PLDMFRUParser<T>::parseFRUTable(const uint8_t* fruTable,
                                    size_t fruTableLength,
                                    size_t recordStartOffset,
                                    DBusHandler* const dbusHandler)
{
    std::vector<uint8_t> fruTableData(fruTable, fruTable + fruTableLength);
    size_t recordDataSize;
    std::unique_ptr<PLDMFRUParser> pldmFRUParser;
    int tlvOffset = 0;
    struct pldm_fru_record_tlv* fruRecordTlv;
    std::string objPath = "/xyz/openbmc_project/pldm/fru_record_set/";
    std::string objectPath = objPath;
    size_t startOffset = recordStartOffset;

    if (fruTableLength == 0 || fruTableLength <= recordStartOffset)
    {
        return PLDM_ERROR_INVALID_LENGTH;
    }

    size_t fruTableLengthTemp = fruTableLength;

    while (fruTableLengthTemp != 0)
    {
        auto fruRecordData =
            reinterpret_cast<struct pldm_fru_record_data_format*>(
                fruTableData.data() + startOffset);
        auto rc = fru_record_data_size_calc(fruTable, fruTableLength,
                                            startOffset, &recordDataSize);
        if (rc != PLDM_SUCCESS)
        {
            return PLDM_ERROR;
        }

        objectPath.append(std::to_string(fruRecordData->record_set_id));
        auto& bus = dbusHandler->getBus();

        pldmFRUParser = std::make_unique<PLDMFRUParser>(bus, objectPath);

        fruParserObjs.emplace_back(std::move(pldmFRUParser.get()));

        for (auto fieldNum = 0; fieldNum < fruRecordData->num_fru_fields;
             fieldNum++)
        {
            fruRecordTlv =
                (struct pldm_fru_record_tlv*)(&fruRecordData->tlvs->type +
                                              tlvOffset);

            dBusMap.propertyName = std::to_string(fruRecordTlv->type);
            dBusMap.propertyType = std::to_string(fruRecordData->record_type);

            uint8_t* value = fruRecordTlv->value;
            dbusHandler->setDbusProperty(dBusMap, PropertyValue{*value});

            tlvOffset += fruRecordTlv->length + sizeof(fruRecordTlv->type) +
                         sizeof(fruRecordTlv->length);
        }
        startOffset += recordDataSize;
        fruTableLengthTemp -= recordDataSize;
        objectPath = objPath;
    }

    return PLDM_SUCCESS;
}

} // namespace fruparser
} // namespace pldm
