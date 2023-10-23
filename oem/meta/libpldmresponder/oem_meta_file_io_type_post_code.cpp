#include "oem_meta_file_io_type_post_code.hpp"

#include <phosphor-logging/lg2.hpp>

#include <utility>

PHOSPHOR_LOG2_USING;
namespace pldm::responder::oem_meta
{

using postcode_t = std::tuple<uint64_t, std::vector<uint8_t>>;

void PostCodeHandler::parseObjectPathToGetSlot(uint64_t& slotNum)
{
    static constexpr auto slotInterface =
        "xyz.openbmc_project.Inventory.Decorator.Slot";
    static constexpr auto slotNumberProperty = "SlotNumber";

    std::vector<std::string> slotInterfaceList = {slotInterface};
    pldm::utils::GetAncestorsResponse response;
    bool found = false;

    for (const auto& [configDbusPath, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId == tid)
        {
            try
            {
                response = pldm::utils::DBusHandler().getAncestors(
                    configDbusPath.c_str(), slotInterfaceList);
            }
            catch (const sdbusplus::exception_t& e)
            {
                error(
                    "GetAncestors call failed with, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
                    "ERROR", e, "PATH", configDbusPath.c_str(), "INTERFACE",
                    slotInterface);
            }

            // It should be only one of the slot property existed.
            if (response.size() != 1)
            {
                error("Get invalide number of slot property, SIZE={SIZE}",
                      "SIZE", response.size());
                return;
            }

            try
            {
                slotNum = pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
                    std::get<0>(response.front()).c_str(), slotNumberProperty,
                    slotInterface);
                found = true;
            }
            catch (const sdbusplus::exception_t& e)
            {
                error(
                    "Error getting Names property, ERROR={ERROR} PATH={PATH} INTERFACE={INTERFACE}",
                    "ERROR", e, "PATH", std::get<0>(response.front()).c_str(),
                    "INTERFACE", slotInterface);
            }
        }
    }

    if (!found)
    {
        throw std::invalid_argument("Configuration of TID not found.");
    }
}

int PostCodeHandler::write(const message& postCodeList)
{
    static constexpr auto dbusService = "xyz.openbmc_project.State.Boot.Raw";
    static constexpr auto dbusObj = "/xyz/openbmc_project/state/boot/raw";
    uint64_t slot;

    try
    {
        parseObjectPathToGetSlot(slot);
    }
    catch (const std::exception& e)
    {
        error("Fail to get the corresponding slot. TID={TID}, ERROR={ERROR}",
              "TID", tid, "ERROR", e);
        return PLDM_ERROR;
    }

    std::string dbusObjStr = dbusObj + std::to_string(slot);

    uint64_t primaryPostCode = 0;

    // Putting list of the bytes together to form a meaningful postcode
    // AMD platform send four bytes as a post code unit
    if (std::cmp_greater(
            std::distance(std::begin(postCodeList), std::end(postCodeList)),
            sizeof(primaryPostCode)))
    {
        error("Get invalid post code length");
        return PLDM_ERROR;
    }

    size_t index = 0;
    std::for_each(postCodeList.begin(), postCodeList.end(),
                  [&primaryPostCode, &index](auto postcode) {
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

        bus.call(method);
    }
    catch (const std::exception& e)
    {
        error("Set Post code error. ERROR={ERROR}", "ERROR", e);
        return PLDM_ERROR;
    }

    return PLDM_SUCCESS;
}

} // namespace pldm::responder::oem_meta
