
#include "oem_meta.hpp"

namespace pldm
{
namespace oem_meta
{

OemMETA::OemMETA(const pldm::utils::DBusHandler* dbusHandler)
{
    configurationDiscovery = std::make_shared<
        pldm::requester::oem_meta::ConfigurationDiscoveryHandler>(dbusHandler);
}

pldm::requester::oem_meta::ConfigurationDiscoveryHandler*
    OemMETA::getMctpConfigurationHandler() const
{
    return configurationDiscovery.get();
}

} // namespace oem_meta
} // namespace pldm
