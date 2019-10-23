#include "fru.hpp"

#include "registration.hpp"

#include <cstring>

namespace pldm
{

namespace responder
{

namespace fru
{

void registerHandlers()
{
    registerHandler(PLDM_FRU, PLDM_GET_FRU_RECORD_TABLE_METADATA,
                    std::move(getFRURecordTableMetadata));
    registerHandler(PLDM_FRU, PLDM_GET_FRU_RECORD_TABLE,
                    std::move(getFRURecordTable));
}

} // namespace fru

namespace internal
{
FruImpl table;
FruIntf<FruImpl> intf(table);
} // namespace internal

Response getFRURecordTableMetadata(const pldm_msg* request,
                                   size_t /*payloadLength*/)
{
    using namespace internal;
    constexpr auto major = 0x01;
    constexpr auto minor = 0x00;
    constexpr auto maxSize = 0xFFFFFFFF;

    Response response(sizeof(pldm_msg_hdr) +
                          PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES,
                      0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    encode_get_fru_record_table_metadata_resp(
        request->hdr.instance_id, PLDM_SUCCESS, major, minor, maxSize,
        intf.size(), intf.numRSI(), intf.numRecords(), intf.checksum(),
        responsePtr);
    return response;
}

Response getFRURecordTable(const pldm_msg* request, size_t payloadLength)
{
    using namespace internal;
    Response response(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_MIN_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_GET_FRU_RECORD_TABLE_REQ_BYTES)
    {
        encode_get_fru_record_table_resp(request->hdr.instance_id,
                                         PLDM_ERROR_INVALID_LENGTH, 0,
                                         PLDM_START_AND_END, responsePtr);
        return response;
    }

    encode_get_fru_record_table_resp(request->hdr.instance_id, PLDM_SUCCESS, 0,
                                     PLDM_START_AND_END, responsePtr);
    intf.populate(response);
    return response;
}

} // namespace responder

inline uint32_t FruImpl::size() const
{
    return sizeof(pldm_fru_record_data_format) - sizeof(uint8_t) + 3;
}

inline uint32_t FruImpl::checksum() const
{
    return 0;
}

inline uint16_t FruImpl::numRSI() const
{
    return 1;
}

inline uint16_t FruImpl::numRecords() const
{
    return 1;
}

void FruImpl::populate(Response& response) const
{
    size_t currSize = 0;
    size_t respHdrSize = response.size();
    response.resize(respHdrSize + size());
    encode_fru_record(response.data() + respHdrSize, size(), &currSize, 0x0001,
                      PLDM_FRU_RECORD_TYPE_GENERAL, 0x01,
                      PLDM_FRU_ENCODING_UNSPECIFIED, nullptr, 5);
    auto tlvStart = response.data() + respHdrSize + currSize;
    auto tlv = reinterpret_cast<pldm_fru_record_tlv*>(tlvStart);
    tlv->type = PLDM_FRU_FIELD_TYPE_VENDOR;
    tlv->length = 3;
    std::memcpy(tlv->value, "IBM", 3);
}

} // namespace pldm
