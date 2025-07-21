#include "file_io_type_post_code.hpp"

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

using postcode_t = std::tuple<std::vector<uint8_t>, std::vector<uint8_t>>;

int PostCodeHandler::write(const message& postCodeList)
{
    static constexpr auto dbusService = "xyz.openbmc_project.State.Boot.Raw";
    static constexpr auto dbusObj = "/xyz/openbmc_project/state/boot/raw";
    static constexpr auto AMD_POST_CODE_LENGTH = 4;

    std::string slotNum;
    try
    {
        slotNum = getSlotNumberStringByTID(configurations, tid);
    }
    catch (const std::exception& e)
    {
        error(
            "Fail to get the corresponding slot with TID {TID} and error code {ERROR} during post code write",
            "TID", tid, "ERROR", e);
        return PLDM_ERROR;
    }

    std::string dbusObjStr = dbusObj + slotNum;

    std::vector<uint8_t> primaryPostCode;

    // Putting list of the bytes together to form a meaningful postcode
    // AMD platform send four bytes as a post code unit
    if (std::cmp_greater(
            std::distance(std::begin(postCodeList), std::end(postCodeList)),
            sizeof(AMD_POST_CODE_LENGTH)))
    {
        error("Get invalid post code length");
        return PLDM_ERROR;
    }

    std::for_each(postCodeList.rbegin(), postCodeList.rend(),
                  [&primaryPostCode](auto postcode) {
                      primaryPostCode.push_back(postcode);
                  });

    try
    {
        auto& bus = pldm::utils::DBusHandler::getBus();
        auto method =
            bus.new_method_call(dbusService, dbusObjStr.c_str(),
                                "org.freedesktop.DBus.Properties", "Set");

        method.append(
            dbusService, "Value",
            std::variant<postcode_t>(postcode_t(primaryPostCode, {})));

        bus.call(method);
    }
    catch (const std::exception& e)
    {
        error("Set Post code error with error code {ERROR}", "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
