#include "utils.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>
PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace oem_meta
{

bool checkMetaIana(const std::map<eid, MctpEndpoint>& configurations,
                   pldm_tid_t tid)
{
    static constexpr std::string MetaIANA = "0015A000";

    if (auto iter = configurations.find(tid); iter != configurations.end())
    {
        const MctpEndpoint& m = iter->second;
        if (!m.iana.has_value())
        {
            lg2::error("message iana do not have value");
            return false;
        }

        if (m.iana.value() != MetaIANA)
        {
            lg2::error("wrong iana {IANA}", "IANA", m.iana.value());
            return false;
        }

        return true;
    }
    return false;
}

std::string getSlotNumberStringByTID(
    const std::map<eid, MctpEndpoint>& configurations, pldm_tid_t tid)
{
    static const pldm::dbus::Property slotNumberProperty = "SlotNumber";
    static const pldm::dbus::Interface slotInterface =
        "xyz.openbmc_project.Inventory.Decorator.Slot";
    static const pldm::dbus::Interfaces slotInterfaces = {slotInterface};

    auto tryHardcodeSlotNumberStringByTID = [tid, &configurations]() {
        for (const auto& [_, m] : configurations)
        {
            if (m.objectPath.find("Yosemite_4") != std::string::npos)
            {
                uint8_t slotNum = tid / 10;
                warning(
                    "Fallback: Found Yosemite_4 in configDbusPath {CONFIG_PATH}, using slot {SLOT} for TID {TID}",
                    "CONFIG_PATH", m.objectPath, "SLOT", slotNum, "TID", tid);
                return std::to_string(slotNum);
            }
        }
        return std::string{};
    };

    pldm::utils::GetAncestorsResponse response;
    if (auto iter = configurations.find(tid); iter != configurations.end())
    {
        const MctpEndpoint& m = iter->second;
        try
        {
            response = pldm::utils::DBusHandler().getAncestors(
                m.objectPath.c_str(), slotInterfaces);
        }
        catch (const sdbusplus::exception_t& e)
        {
            error(
                "GetAncestors call failed with error code {ERROR}, path {PATH} and interface {INTERFACE}",
                "ERROR", e, "PATH", m.objectPath.c_str(), "INTERFACE",
                slotInterface);
            return tryHardcodeSlotNumberStringByTID();
        }
        if (response.size() != 1)
        {
            error("Get invalide number {SIZE} of slot property", "SIZE",
                  response.size());
            return tryHardcodeSlotNumberStringByTID();
        }
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
        return tryHardcodeSlotNumberStringByTID();
    }

    return std::to_string(slotNum);
}

} // namespace oem_meta
} // namespace pldm
