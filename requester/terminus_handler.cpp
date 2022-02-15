#include "config.h"

#include "terminus_handler.hpp"

#include "libpldm/requester/pldm.h"

namespace pldm
{

namespace terminus
{

TerminusHandler::TerminusHandler(
    uint8_t tid, sdeventplus::Event& event, sdbusplus::bus::bus& bus,
    pldm_pdr* repo, pldm_entity_association_tree* entityTree,
    pldm_entity_association_tree* bmcEntityTree,
    pldm::dbus_api::Requester& requester,
    pldm::requester::Handler<pldm::requester::Request>* handler) :
    tid(tid),
    bus(bus), event(event), repo(repo), entityTree(entityTree),
    bmcEntityTree(bmcEntityTree), requester(requester), handler(handler)
{}

} // namespace terminus

} // namespace pldm
