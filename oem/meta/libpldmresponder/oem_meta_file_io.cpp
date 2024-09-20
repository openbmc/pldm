#include "oem_meta_file_io.hpp"

#include "oem_meta_file_io_type_bios_version.hpp"
#include "oem_meta_file_io_type_http_boot.hpp"
#include "oem_meta_file_io_type_post_code.hpp"
#include "oem_meta_file_io_type_power_control.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/oem/meta/file_io.h>

#include <filesystem>
namespace pldm::responder::oem_meta
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;

static constexpr auto mctpEndpointInterface =
    "xyz.openbmc_project.Configuration.MCTPEndpoint";

std::unique_ptr<FileHandler> FileIOHandler::getHandlerByType(uint8_t messageTid,
                                                             uint8_t fileIOType)
{
    switch (fileIOType)
    {
        case POST_CODE:
            return std::make_unique<PostCodeHandler>(
                messageTid, configurationDiscovery->getConfigurations());
        case BIOS_VERSION:
            return std::make_unique<BIOSVersionHandler>(
                messageTid, configurationDiscovery->getConfigurations(),
                dBusIntf);
        case POWER_CONTROL:
            return std::make_unique<PowerControlHandler>(
                messageTid, configurationDiscovery->getConfigurations(),
                dBusIntf);
        case HTTP_BOOT:
            return std::make_unique<HttpBootHandler>(
                messageTid, configurationDiscovery->getConfigurations());
        default:
            error("Get invalid file io type, FILEIOTYPE={FILEIOTYPE}",
                  "FILEIOTYPE", fileIOType);
            break;
    }
    return nullptr;
}

Response FileIOHandler::writeFileIO(pldm_tid_t tid, const pldm_msg* request,
                                    size_t payloadLength)
{
    constexpr uint8_t decodereqlen =
        PLDM_OEM_META_FILE_IO_WRITE_REQ_MIN_LENGTH + decodeDataMaxLength;
    alignas(struct pldm_oem_meta_file_io_write_req) unsigned char
        reqbuf[decodereqlen];
    auto request_msg = new (reqbuf) pldm_oem_meta_file_io_write_req;
    auto request_data = static_cast<uint8_t*>(
        pldm_oem_meta_file_io_write_req_data(request_msg));

    int rc = decode_oem_meta_file_io_write_req(request, payloadLength,
                                               request_msg, decodereqlen);

    if (rc != 0)
    {
        error("Failed to decode OEM Meta file IO request, rc={RC}", "RC", rc);
        return ccOnlyResponse(request, rc);
    }

    auto handler = getHandlerByType(tid, request_msg->handle);

    if (!handler)
    {
        return ccOnlyResponse(request, PLDM_ERROR_UNSUPPORTED_PLDM_CMD);
    }

    auto data = std::span(request_data, request_msg->length);

    rc = handler->write(data);

    return ccOnlyResponse(request, rc);
}

Response FileIOHandler::readFileIO(pldm_tid_t tid, const pldm_msg* request,
                                   size_t payloadLength)
{
    int rc;
    struct pldm_oem_meta_file_io_read_req request_msg = {};
    request_msg.version = sizeof(struct pldm_oem_meta_file_io_read_req);

    rc = decode_oem_meta_file_io_read_req(request, payloadLength, &request_msg);

    if (rc != 0)
    {
        error("Failed to decode OEM read file IO request, rc={RC}", "RC", rc);
        return ccOnlyResponse(request, rc);
    }

    auto handler = getHandlerByType(tid, request_msg.handle);

    if (!handler)
    {
        return ccOnlyResponse(request, PLDM_ERROR_UNSUPPORTED_PLDM_CMD);
    }

    size_t encoderesplen = PLDM_OEM_META_FILE_IO_READ_RESP_MIN_SIZE;
    if (request_msg.option == PLDM_OEM_META_FILE_IO_READ_ATTR)
    {
        encoderesplen += PLDM_OEM_META_FILE_IO_READ_ATTR_INFO_LENGTH;
    }
    else
    {
        encoderesplen += PLDM_OEM_META_FILE_IO_READ_DATA_INFO_LENGTH;
    }

    std::vector<uint8_t> respbuf(
        sizeof(struct pldm_oem_meta_file_io_read_resp) + request_msg.length);
    auto* response_msg = new (respbuf.data()) pldm_oem_meta_file_io_read_resp;

    response_msg->version = sizeof(struct pldm_oem_meta_file_io_read_resp);
    response_msg->handle = request_msg.handle;
    response_msg->option = request_msg.option;
    response_msg->length = request_msg.length;

    if (response_msg->option != PLDM_OEM_META_FILE_IO_READ_ATTR)
    {
        response_msg->info.data.transferFlag =
            request_msg.info.data.transferFlag;
        response_msg->info.data.offset = request_msg.info.data.offset;
    }

    rc = handler->read(response_msg);

    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    response_msg->completion_code = PLDM_SUCCESS;
    encoderesplen += response_msg->length;

    Response response(sizeof(pldm_msg_hdr) + encoderesplen, 0);
    auto responseMsg = new (response.data()) pldm_msg;

    rc = encode_oem_meta_file_io_read_resp(
        request->hdr.instance_id, response_msg,
        sizeof(struct pldm_oem_meta_file_io_read_resp) + response_msg->length,
        responseMsg, encoderesplen);

    if (rc != 0)
    {
        return ccOnlyResponse(request, rc);
    }

    return response;
}

} // namespace pldm::responder::oem_meta
