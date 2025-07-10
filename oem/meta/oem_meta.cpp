
#include "oem_meta.hpp"

namespace pldm::oem_meta
{

OemMETA::OemMETA(const pldm::utils::DBusHandler* dbusHandler)
{
    utils::initConfigurationDiscoveryHandler(dbusHandler);
}

ConfigurationDiscoveryHandler* OemMETA::getMctpConfigurationHandler() const
{
    return utils::getMctpConfigurationHandler();
}

} // namespace pldm::oem_meta
