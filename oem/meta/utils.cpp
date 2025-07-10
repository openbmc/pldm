#include "utils.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>
PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace oem_meta
{

bool checkMetaIana(
    pldm_tid_t tid,
    const std::map<pldm::dbus::ObjectPath, MctpEndpoint>& configurations)
{
    static constexpr std::string MetaIANA = "0015A000";

    for (const auto& [_, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId == tid)
        {
            if (mctpEndpoint.iana.has_value() &&
                mctpEndpoint.iana.value() == MetaIANA)
            {
                return true;
            }
            else if (mctpEndpoint.iana.value() != MetaIANA)
            {
                lg2::error("WRONG IANA {IANA}", "IANA",
                           mctpEndpoint.iana.value());
                return false;
            }
        }
    }
    return false;
}

uint64_t getSlotNumberByTID(
    const std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>&
        configurations,
    pldm_tid_t tid)
{
    static const pldm::dbus::Property slotNumberProperty = "SlotNumber";
    static const pldm::dbus::Interface slotInterface =
        "xyz.openbmc_project.Inventory.Decorator.Slot";
    static const pldm::dbus::Interfaces slotInterfaces = {slotInterface};

    pldm::utils::GetAncestorsResponse response;

    bool getAncestorsSucc = false;
    for (const auto& [path, mctpEndpoint] : configurations)
    {
        if (mctpEndpoint.EndpointId != tid)
        {
            continue;
        }

        try
        {
            response = pldm::utils::DBusHandler().getAncestors(path.c_str(),
                                                               slotInterfaces);
        }
        catch (const sdbusplus::exception_t& e)
        {
            error(
                "GetAncestors call failed with error code {ERROR}, path {PATH} and interface {INTERFACE}",
                "ERROR", e, "PATH", path.c_str(), "INTERFACE", slotInterface);
            break;
        }

        // It should be only one of the slot property existed.
        if (response.size() != 1)
        {
            error("Get invalide number {SIZE} of slot property", "SIZE",
                  response.size());
            break;
        }

        getAncestorsSucc = true;
        break; // tid should be unique
    }
    if (!getAncestorsSucc)
    {
        throw std::invalid_argument("Configuration of TID not found.");
    }

    uint64_t slotNum = 0;
    try
    {
        slotNum = pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
            std::get<0>(response.front()).c_str(), slotNumberProperty.c_str(),
            slotInterface.c_str());
    }
    catch (const sdbusplus::exception_t& e)
    {
        warning(
            "Getting SlotNumber property with warning {WARNING}, path {PATH} and interface {INTERFACE}",
            "WARNING", e, "PATH", std::get<0>(response.front()).c_str(),
            "INTERFACE", slotInterface);
        throw std::invalid_argument("Configuration of TID not found.");
    }

    return slotNum;
}

} // namespace oem_meta
} // namespace pldm
