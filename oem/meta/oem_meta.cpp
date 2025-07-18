
#include "oem_meta.hpp"

#include <utility>

namespace pldm
{
namespace oem_meta
{

OemMETA::OemMETA(const pldm::utils::DBusHandler* dbusHandler,
                 pldm::responder::Invoker& invoker,
                 pldm::responder::platform::Handler* platformHandler)
{
    configurationDiscovery = std::make_shared<
        pldm::requester::oem_meta::ConfigurationDiscoveryHandler>(dbusHandler);

    oemEventManager =
        std::make_unique<oem_meta::OemEventManager>(configurationDiscovery);
    registerOemEventHandler(platformHandler);

		auto fileIOHandler =
        std::make_unique<pldm::responder::oem_meta::FileIOHandler>(
            configurationDiscovery, dbusHandler);
    registerOemHandler(invoker, std::move(fileIOHandler));
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

void OemMETA::registerOemHandler(pldm::responder::Invoker& invoker, std::unique_ptr<pldm::responder::oem_meta::FileIOHandler> fileIOHandler)
{
    invoker.registerHandler(PLDM_OEM, std::move(fileIOHandler));
}

} // namespace oem_meta
} // namespace pldm
