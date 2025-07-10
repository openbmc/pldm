
#include "oem_meta.hpp"

namespace pldm
{
namespace oem_meta
{

OemMETA::OemMETA(const pldm::utils::DBusHandler* dbusHandler)
{
    utils::initConfigurationDiscoveryHandler(dbusHandler);
}

ConfigurationDiscoveryHandler* OemMETA::getMctpConfigurationHandler() const
{
    return utils::getMctpConfigurationHandler();
}

} // namespace oem_meta
} // namespace pldm
