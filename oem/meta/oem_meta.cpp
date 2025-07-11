
#include "oem_meta.hpp"

namespace pldm
{
namespace oem_meta
{

OemMETA::OemMETA(const pldm::utils::DBusHandler* dbusHandler,
                 pldm::responder::platform::Handler* platformHandler)
{
    configurationDiscovery = std::make_shared<
        pldm::requester::oem_meta::ConfigurationDiscoveryHandler>(dbusHandler);

    oemEventManager =
        std::make_shared<oem_meta::OemEventManager>(configurationDiscovery);
    registerOemEventHandler(platformHandler);
}

pldm::requester::oem_meta::ConfigurationDiscoveryHandler*
    OemMETA::getMctpConfigurationHandler() const
{
    return configurationDiscovery.get();
}

void OemMETA::registerOemEventHandler(
    pldm::responder::platform::Handler* platformHandler)
{
    platformHandler->registerEventHandlers(
        PLDM_OEM_EVENT_CLASS_0xFB,
        {[this](const pldm_msg* request, size_t payloadLength,
                uint8_t formatVersion, uint8_t tid, size_t eventDataOffset) {
            return this->oemEventManager->handleOemEvent(
                request, payloadLength, formatVersion, tid, eventDataOffset);
        }});
}

} // namespace oem_meta
} // namespace pldm
