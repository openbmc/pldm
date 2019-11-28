
#include "platform.hpp"

namespace pldm
{
namespace responder
{
namespace platform
{

using namespace phosphor::logging;
using namespace pldm::responder::effecter::dbus_mapping;

Response Handler::getPDR(const pldm_msg* request, size_t payloadLength)
{
    Response response(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_MIN_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_GET_PDR_REQ_BYTES)
    {
        encode_get_pdr_resp(request->hdr.instance_id, PLDM_ERROR_INVALID_LENGTH,
                            0, 0, 0, 0, nullptr, 0, responsePtr);
        return response;
    }

    uint32_t recordHandle{};
    uint32_t dataTransferHandle{};
    uint8_t transferOpFlag{};
    uint16_t reqSizeBytes{};
    uint16_t recordChangeNum{};

    decode_get_pdr_req(request, payloadLength, &recordHandle,
                       &dataTransferHandle, &transferOpFlag, &reqSizeBytes,
                       &recordChangeNum);

    uint32_t nextRecordHandle{};
    uint16_t respSizeBytes{};
    uint8_t* recordData = nullptr;
    try
    {
        pdr::Repo& pdrRepo = pdr::get(PDR_JSONS_DIR);
        nextRecordHandle = pdrRepo.getNextRecordHandle(recordHandle);
        pdr::Entry e;
        if (reqSizeBytes)
        {
            e = pdrRepo.at(recordHandle);
            respSizeBytes = e.size();
            if (respSizeBytes > reqSizeBytes)
            {
                respSizeBytes = reqSizeBytes;
            }
            recordData = e.data();
        }
        response.resize(sizeof(pldm_msg_hdr) + PLDM_GET_PDR_MIN_RESP_BYTES +
                            respSizeBytes,
                        0);
        responsePtr = reinterpret_cast<pldm_msg*>(response.data());
        encode_get_pdr_resp(request->hdr.instance_id, PLDM_SUCCESS,
                            nextRecordHandle, 0, PLDM_START, respSizeBytes,
                            recordData, 0, responsePtr);
    }
    catch (const std::out_of_range& e)
    {
        encode_get_pdr_resp(request->hdr.instance_id,
                            PLDM_PLATFORM_INVALID_RECORD_HANDLE,
                            nextRecordHandle, 0, PLDM_START, respSizeBytes,
                            recordData, 0, responsePtr);
        return response;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error accessing PDR", entry("HANDLE=%d", recordHandle),
                        entry("ERROR=%s", e.what()));
        encode_get_pdr_resp(request->hdr.instance_id, PLDM_ERROR,
                            nextRecordHandle, 0, PLDM_START, respSizeBytes,
                            recordData, 0, responsePtr);
        return response;
    }
    return response;
}

Response Handler::setStateEffecterStates(const pldm_msg* request,
                                         size_t payloadLength)
{
    Response response(
        sizeof(pldm_msg_hdr) + PLDM_SET_STATE_EFFECTER_STATES_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    uint16_t effecterId;
    uint8_t compEffecterCnt;
    constexpr auto maxCompositeEffecterCnt = 8;
    std::vector<set_effecter_state_field> stateField(maxCompositeEffecterCnt,
                                                     {0, 0});

    if ((payloadLength > PLDM_SET_STATE_EFFECTER_STATES_REQ_BYTES) ||
        (payloadLength < sizeof(effecterId) + sizeof(compEffecterCnt) +
                             sizeof(set_effecter_state_field)))
    {
        encode_set_state_effecter_states_resp(
            request->hdr.instance_id, PLDM_ERROR_INVALID_LENGTH, responsePtr);
        return response;
    }

    int rc = decode_set_state_effecter_states_req(request, payloadLength,
                                                  &effecterId, &compEffecterCnt,
                                                  stateField.data());

    if (rc == PLDM_SUCCESS)
    {
        stateField.resize(compEffecterCnt);
        const DBusHandler dBusIntf;
        rc = setStateEffecterStatesHandler<DBusHandler>(dBusIntf, effecterId,
                                                        stateField);
    }

    encode_set_state_effecter_states_resp(request->hdr.instance_id, rc,
                                          responsePtr);
    return response;
}

} // namespace platform
} // namespace responder
} // namespace pldm
