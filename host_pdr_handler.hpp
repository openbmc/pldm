#pragma once

#include "dbus_impl_requester.hpp"
#include "utils.hpp"

#include <map>
#include <memory>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/platform.h"

using namespace pldm::dbus_api;

using EntityType = uint16_t;
// vector which would hold the PDR record handle data returned by
// pldmPDRRepositoryChgEvent evend data
using ChangeEntry = uint32_t;
using PDRRecordHandles = std::vector<ChangeEntry>;

namespace pldm
{

class HostPDRHandler
{
  public:
    HostPDRHandler() = delete;
    HostPDRHandler(HostPDRHandler&) = delete;
    HostPDRHandler(HostPDRHandler&&) = delete;
    HostPDRHandler& operator=(const HostPDRHandler&) = delete;
    HostPDRHandler& operator=(HostPDRHandler&&) = delete;
    ~HostPDRHandler() = default;

    HostPDRHandler(int mctp_fd, uint8_t mctp_eid, sdeventplus::Event& event,
                   pldm_pdr* repo, pldm_entity_association_tree* entityTree,
                   Requester& requester);

    /**@brief fetch remote PDRs based on the recordHandles
     *@param[in] recordHandles - a vector of recordHandles
     */
    void fetchPDR(std::vector<uint32_t>&& recordHandles);

  private:
    void _fetchPDR(sdeventplus::source::EventBase& source);

    void mergeEntityAssociations(const std::vector<uint8_t>& pdr);

    bool getParent(EntityType type, pldm_entity& parent);

    int mctp_fd;
    uint8_t mctp_eid;
    sdeventplus::Event& event;
    pldm_pdr* repo;
    pldm_entity_association_tree* entityTree;
    Requester& requester;
    std::unique_ptr<sdeventplus::source::Defer> pdrFetcherEventSrc;
    PDRRecordHandles pdrRecordHandles;
    std::map<EntityType, pldm_entity> parents;
};

} // namespace pldm
