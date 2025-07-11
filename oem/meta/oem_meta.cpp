
#include "oem_meta.hpp"

#include <libpldm/base.h>

namespace pldm::oem_meta
{

OemMETA::OemMETA(const pldm::utils::DBusHandler* dbusHandler,
                 pldm::responder::platform::Handler* platformHandler) :
    dbusHandler(dbusHandler)
{
    oemEventManager = std::make_unique<oem_meta::OemEventManager>();
    registerOemEventHandler(platformHandler);
}

void OemMETA::registerOemEventHandler(
    pldm::responder::platform::Handler* platformHandler)
{
    platformHandler->registerEventHandlers(
        PLDM_OEM_EVENT_CLASS_0xFB,
        {[this](const pldm_msg* request, size_t payloadLength,
                uint8_t formatVersion, pldm_tid_t tid, size_t eventDataOffset) {
            return this->oemEventManager->handleOemEvent(
                request, payloadLength, formatVersion, tid, eventDataOffset);
        }});
}

} // namespace pldm::oem_meta
