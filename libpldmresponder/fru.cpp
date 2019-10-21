#include "fru.hpp"

#include "registration.hpp"

namespace pldm
{

namespace responder
{

namespace fru
{

void registerHandlers()
{
    registerHandler(PLDM_FRU, PLDM_GET_FRU_RECORD_TABLE_METADATA, std::move(getFRURecordTableMetadata));
}

} // namespace fru

Response getFRURecordTableMetadata(const pldm_msg* request, size_t /*payloadLength*/)
{
    constexpr auto major = 0x01;
    constexpr auto minor = 0x00;
    constexpr auto maxSize = 0xFFFFFFFF;

    Response response(
        sizeof(pldm_msg_hdr) + PLDM_GET_FRU_RECORD_TABLE_METADATA_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    encode_get_fru_record_table_metadata_resp(request->hdr.instance_id, PLDM_SUCCESS, major, minor, maxSize, 0,0,0,0,
                                          responsePtr);
    return response;
}

} // namespace responder
} // namespace pldm
