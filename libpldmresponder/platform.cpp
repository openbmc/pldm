#include "platform.hpp"
#include "pdr.hpp"
#include "config.h"
#include <exception>
#include <phosphor-logging/log.hpp>

namespace pldm
{

namespace responder
{

using namespace phosphor::logging;

void getPDR(const pldm_msg_payload* request, pldm_msg* response)
{
    if (request->payload_length != PLDM_GET_PDR_REQ_BYTES)
    {
        encode_get_commands_resp(0, PLDM_ERROR_INVALID_LENGTH, nullptr,
                                 response);
        return;
    }

    uint32_t recordHandle{};
    uint32_t dataTransferHandle{};
    uint8_t transferOpFlag{};
    uint16_t reqSizeBytes{};
    uint16_t recordChangeNum{};
    decode_get_pdr_req(request, &recordHandle, &dataTransferHandle, &transferOpFlag, &reqSizeBytes, &recordChangeNum);

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
        encode_get_pdr_resp(0, PLDM_SUCCESS, nextRecordHandle, 0, PLDM_START, respSizeBytes, recordData, 0, response);
    }
    catch (const std::out_of_range& e)
    {
        encode_get_pdr_resp(0, PLDM_PLATFORM_INVALID_RECORD_HANDLE, nextRecordHandle, 0, PLDM_START, respSizeBytes, recordData, 0, response);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error accessing PDR", entry("HANDLE=%d", bmcTimePath), entry("ERROR=%s", e.what()));
        encode_get_pdr_resp(0, PLDM_ERROR, nextRecordHandle, 0, PLDM_START, respSizeBytes, recordData, 0, response);
    }
}

} // namespace responder
} // namespace pldm
