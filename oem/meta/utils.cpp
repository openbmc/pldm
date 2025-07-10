#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
PHOSPHOR_LOG2_USING;

namespace pldm::oem_meta
{

namespace
{

/* @brief MCTP configurations handler */
std::unique_ptr<ConfigurationDiscoveryHandler> configurationDiscovery{};

/* @brief map PLDM TID to MCTP EID; currently, they have a one-to-one
 *        correspondence
 */
eid mapTIDtoEID(pldm_tid_t tid)
{
    return static_cast<eid>(tid);
}

} // namespace

namespace utils
{

void initConfigurationDiscoveryHandler(
    const pldm::utils::DBusHandler* dbusHandler)
{
    configurationDiscovery =
        std::make_unique<ConfigurationDiscoveryHandler>(dbusHandler);
}

ConfigurationDiscoveryHandler* getMctpConfigurationHandler()
{
    return configurationDiscovery.get();
}

} // namespace utils

bool checkMetaIana(pldm_tid_t tid)
{
    static constexpr std::string MetaIANA = "0015A000";

    eid EID = mapTIDtoEID(tid);
    auto configurationIter =
        configurationDiscovery->getConfigurations().find(EID);
    if (configurationIter == configurationDiscovery->getConfigurations().end())
    {
        return false;
    }

    const MctpEndpoint& mctpEndpoint = configurationIter->second;
    if (!mctpEndpoint.iana.has_value())
    {
        lg2::error("message iana do not have value");
        return false;
    }

    if (mctpEndpoint.iana.value() != MetaIANA)
    {
        lg2::error("wrong iana {IANA}", "IANA", mctpEndpoint.iana.value());
        return false;
    }

    return true;
}

std::string getSlotNumberStringByTID(pldm_tid_t tid)
{
    static const pldm::dbus::Property slotNumberProperty = "SlotNumber";
    static const pldm::dbus::Interface slotInterface =
        "xyz.openbmc_project.Inventory.Decorator.Slot";
    static const pldm::dbus::Interfaces slotInterfaces = {slotInterface};

    auto tryHardcodeSlotNumberStringByTID = [tid]() {
        for (const auto& [_, mctpEndpoint] :
             configurationDiscovery->getConfigurations())
        {
            if (mctpEndpoint.objectPath.find("Yosemite_4") != std::string::npos)
            {
                uint8_t slotNum = tid / 10;
                warning(
                    "Fallback: Found Yosemite_4 in configDbusPath {CONFIG_PATH}, using slot {SLOT} for TID {TID}",
                    "CONFIG_PATH", mctpEndpoint.objectPath, "SLOT", slotNum,
                    "TID", tid);
                return std::to_string(slotNum);
            }
        }
        return std::string{"0"};
    };

    eid EID = mapTIDtoEID(tid);
    auto configurationIter =
        configurationDiscovery->getConfigurations().find(EID);
    if (configurationIter == configurationDiscovery->getConfigurations().end())
    {
        return std::string{"0"};
    }

    pldm::utils::GetAncestorsResponse response;
    const MctpEndpoint& mctpEndpoint = configurationIter->second;
    try
    {
        response = pldm::utils::DBusHandler().getAncestors(
            mctpEndpoint.objectPath.c_str(), slotInterfaces);
    }
    catch (const sdbusplus::exception_t& e)
    {
        error(
            "GetAncestors call failed with error code {ERROR}, path {PATH} and interface {INTERFACE}",
            "ERROR", e, "PATH", mctpEndpoint.objectPath.c_str(), "INTERFACE",
            slotInterface);
        return tryHardcodeSlotNumberStringByTID();
    }
    if (response.size() != 1)
    {
        error("Get invalide number {SIZE} of slot property", "SIZE",
              response.size());
        return tryHardcodeSlotNumberStringByTID();
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

} // namespace pldm::oem_meta
