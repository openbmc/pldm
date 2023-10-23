#include "file_io_type_post_code.hpp"

// #include <filesystem>

namespace pldm
{
namespace responder
{

namespace oem_meta
{

using postcode_t = std::tuple<uint64_t, std::vector<uint8_t>>;

int postCodeHandler(
    int slot, const std::array<uint8_t, decodeDataMaxLength>& retDataField,
    uint32_t length)
{
    static constexpr auto dbusService = "xyz.openbmc_project.State.Boot.Raw";
    static constexpr auto dbusObj = "/xyz/openbmc_project/state/boot/raw";

    std::string dbusObjStr = dbusObj + std::to_string(slot);

    uint64_t primaryPostCode = 0;

    std::vector<uint8_t> postCodeList(retDataField.begin(),
                                      retDataField.begin() + length);

    // Putting list of the bytes together to form a meaningful postcode
    // AMD platform send four bytes as a post code unit
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
        error("Set Post code error. ERROR={ERROR}", "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace oem_meta

} // namespace responder
} // namespace pldm
