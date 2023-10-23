#include "file_io.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <libpldm/oem/meta/file_io.h>

#include <phosphor-logging/lg2.hpp>

#include <filesystem>

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace responder
{

namespace oem_meta
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;

using postcode_t = std::tuple<uint64_t, std::vector<uint8_t>>;

static std::map<uint8_t, char> tidToSlotMap;

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
            "ERROR", e.what(), "PATH", searchpath, "INTERFACE",
            mctpEndpointInterface);
        return PLDM_ERROR;
    }

    static constexpr auto endpointIdsProperty = "EndpointId";

    for (const auto& [objectPath, serviceMap] : response)
    {
        // Split the objectPath to get the correspond slot.
        // Yosemite_4_Wailua_Falls_Slot4/BIC;
        char slotNum = std::filesystem::path(objectPath.c_str())
                           .parent_path()
                           .string()
                           .back();

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
                "ERROR", e.what(), "PATH", searchpath, "INTERFACE",
                mctpEndpointInterface);
            return PLDM_ERROR;
        }
    }

    return PLDM_SUCCESS;
}

int postCodeHandler(char slot, std::vector<uint8_t>& postCodeList)
{
    static constexpr auto dbusService = "xyz.openbmc_project.State.Boot.Raw";
    static constexpr auto dbusObj = "/xyz/openbmc_project/state/boot/raw";

    std::string dbusObjStr = dbusObj + slot;

    uint64_t primaryPostCode = 0;

    // Putting list of the bytes together to form a meaningful postcode
    size_t index = 0;
    std::for_each(postCodeList.begin(), postCodeList.end(),
                  [&primaryPostCode, &index](uint8_t postcode) {
        primaryPostCode |= std::uint64_t(postcode) << (8 * index);
        index++;
    });

    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        auto method = bus.new_method_call(dbusService, dbusObjStr.c_str(),
                                          "org.freedesktop.DBus.Properties",
                                          "Set");

        method.append(
            dbusService, "Value",
            std::variant<postcode_t>(postcode_t(primaryPostCode, {})));

        auto reply = bus.call(method);
    }
    catch (const std::exception& e)
    {
        error("Set Post code error. ERROR={ERROR}", "ERROR", e.what());
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

Response Handler::writeFileIO(const pldm_msg* request, size_t payloadLength)
{
    uint8_t fileIOType;
    uint32_t length;
    constexpr auto decodeDataMaxLength = 32;

    std::array<uint8_t, decodeDataMaxLength> retDataField{};

    auto rc = decode_oem_meta_file_io_req(request, payloadLength, &fileIOType,
                                          &length, retDataField.data());

    if (rc != PLDM_SUCCESS)
    {
        return ccOnlyResponse(request, rc);
    }

    if (fileIOType == POST_CODE)
    {
        std::vector<uint8_t> postCodeList(retDataField.begin(),
                                          retDataField.begin() + length);

        rc = postCodeHandler(tidToSlotMap[this->getTID()], postCodeList);

        if (rc != PLDM_SUCCESS)
        {
            return ccOnlyResponse(request, rc);
        }
    }

    return ccOnlyResponse(request, rc);
}
} // namespace oem_meta

} // namespace responder
} // namespace pldm
