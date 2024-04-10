#include "oem_meta_file_io.hpp"

#include "oem_meta_file_io_type_bios_version.hpp"
#include "oem_meta_file_io_type_post_code.hpp"
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

    auto rc = decode_oem_meta_file_io_req(request, payloadLength, &fileIOType,
                                          &length, retDataField.data());

    if (rc != PLDM_SUCCESS)
    {
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

} // namespace pldm::responder::oem_meta
