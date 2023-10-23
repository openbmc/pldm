#include "file_io.hpp"

#include "file_io_type_post_code.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/oem/meta/file_io.h>

#include <filesystem>

namespace pldm
{
namespace responder
{

namespace oem_meta
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;

static constexpr auto mctpEndpointInterface =
    "xyz.openbmc_project.Configuration.MCTPEndpoint";

int setupTidToSlotMappingTable()
{
    static constexpr auto searchpath = "/xyz/openbmc_project/";
    int depth = 0;
    std::vector<std::string> mctpInterface = {mctpEndpointInterface};
    pldm::utils::GetSubTreeResponse response;
    try
    {
        response = pldm::utils::DBusHandler().getSubtree(searchpath, depth,
                                                         mctpInterface);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "getSubtree call failed with, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
            "ERROR", e, "PATH", searchpath, "INTERFACE", mctpEndpointInterface);
        return PLDM_ERROR;
    }

    static constexpr auto endpointIdsProperty = "EndpointId";

    for (const auto& [objectPath, serviceMap] : response)
    {
        // Split the objectPath to get the correspond slot.
        // e.g Yosemite_4_Wailua_Falls_Slot3/BIC;
        int slotNum = std::filesystem::path(objectPath.c_str())
                          .parent_path()
                          .string()
                          .back() -
                      '0';

        try
        {
            auto value = pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
                objectPath.c_str(), endpointIdsProperty, mctpEndpointInterface);

            tidToSlotMap.insert({value, slotNum});
        }
        catch (const sdbusplus::exception_t& e)
        {
            error(
                "Error getting Names property, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
                "ERROR", e, "PATH", searchpath, "INTERFACE",
                mctpEndpointInterface);
            return PLDM_ERROR;
        }
    }

    return PLDM_SUCCESS;
}

Response Handler::writeFileIO(const pldm_msg* request, size_t payloadLength)
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

    switch (fileIOType)
    {
        case POST_CODE:
            rc = postCodeHandler(tidToSlotMap[this->getTID()], retDataField,
                                 length);
            break;
        default:
            rc = PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    return ccOnlyResponse(request, rc);
}
} // namespace oem_meta

} // namespace responder
} // namespace pldm
