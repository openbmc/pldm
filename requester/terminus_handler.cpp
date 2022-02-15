#include "config.h"

#include "terminus_handler.hpp"

#include "libpldm/requester/pldm.h"

namespace pldm
{

namespace terminus
{

TerminusHandler::TerminusHandler(
    int mctp_fd, uint8_t mctp_eid, sdeventplus::Event& event,
    sdbusplus::bus::bus& bus, pldm_pdr* repo,
    pldm_entity_association_tree* entityTree,
    pldm_entity_association_tree* bmcEntityTree,
    pldm::dbus_api::Requester& requester,
    pldm::requester::Handler<pldm::requester::Request>* handler) :
    mctp_fd(mctp_fd),
    mctp_eid(mctp_eid), bus(bus), event(event), repo(repo),
    entityTree(entityTree), bmcEntityTree(bmcEntityTree), requester(requester),
    handler(handler)
{}

} // namespace terminus

} // namespace pldm
