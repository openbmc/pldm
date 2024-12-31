#include "oem_meta_file_io.hpp"

#include "oem_meta_file_io_type_bios_version.hpp"
#include "oem_meta_file_io_type_crash_dump.hpp"
#include "oem_meta_file_io_type_event_log.hpp"
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
        case EVENT_LOG:
            return std::make_unique<EventLogHandler>(
                messageTid, configurationDiscovery->getConfigurations());
        case CRASH_DUMP:
            return std::make_unique<CrashDumpHandler>(
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
    uint8_t fileIOType;
    uint32_t length;

    std::array<uint8_t, decodeDataMaxLength> retDataField{};

    auto rc = decode_oem_meta_write_file_io_req(
        request, payloadLength, &fileIOType, &length, retDataField.data());

    if (rc != PLDM_SUCCESS)
    {
        error("Failed to decode OEM Meta file IO request, rc={RC}", "RC", rc);
        return ccOnlyResponse(request, rc);
    }

    auto handler = getHandlerByType(tid, fileIOType);

    if (!handler)
    {
        return ccOnlyResponse(request, PLDM_ERROR_UNSUPPORTED_PLDM_CMD);
    }

    auto data = std::span(std::begin(retDataField), length);

    rc = handler->write(data);

    return ccOnlyResponse(request, rc);
}

Response FileIOHandler::readFileIO(pldm_tid_t tid, const pldm_msg* request,
                                   size_t payloadLength)
{
    uint8_t completionCode = PLDM_SUCCESS;
    struct pldm_oem_meta_read_file_req req;

    auto rc = decode_oem_meta_read_file_io_req(request, payloadLength, &req);

    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    auto handler = getHandlerByType(tid, req.file_handle);

    if (!handler)
    {
        return ccOnlyResponse(request, PLDM_ERROR_UNSUPPORTED_PLDM_CMD);
    }

    std::vector<uint8_t> data = {req.length, req.transferFlag, req.highOffset,
                                 req.lowOffset};

    rc = handler->read(data);

    Response response(sizeof(pldm_msg_hdr) + sizeof(completionCode), 0);

    auto responseMsg = reinterpret_cast<pldm_msg*>(response.data());

    rc = encode_http_boot_header_resp(request->hdr.instance_id, completionCode,
                                      responseMsg);

    response.insert(response.begin() + sizeof(pldm_msg_hdr) +
                        sizeof(completionCode),
                    data.begin(), data.end());

    return response;
}

} // namespace pldm::responder::oem_meta
