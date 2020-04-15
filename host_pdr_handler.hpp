#pragma once

#include "dbus_impl_requester.hpp"
#include "utils.hpp"

#include <memory>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/platform.h"

using namespace pldm::dbus_api;

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
                   pldm_pdr* repo, Requester& requester) :
        mctp_fd(mctp_fd),
        mctp_eid(mctp_eid), event(event), repo(repo), requester(requester)
    {
    }

    /**@brief fetch remote PDRs based on the recordHandles
     *@param[in] recordHandles - a vector of recordHandles
     */

    void fetchPDR(std::vector<uint32_t>&& recordHandles);

    /**brief preparepldmPDRRepositoryChgEventData based on the pdrTypes
     *@param[in] pdrTypes - a vector of PDR Types
     *@param[in] eventDataFormat - format of this eventData
     *@param[in] repo - opaque pointer acting as PDR repo handle
     */
    std::vector<uint8_t> preparepldmPDRRepositoryChgEventData(
        const std::vector<uint8_t>& pdrTypes, uint8_t eventDataFormat,
        const pldm_pdr* repo);

  private:
    void _fetchPDR(sdeventplus::source::EventBase& source);

    int mctp_fd;
    uint8_t mctp_eid;
    sdeventplus::Event& event;
    pldm_pdr* repo;
    Requester& requester;
    std::unique_ptr<sdeventplus::source::Defer> pdrFetcherEventSrc;
    PDRRecordHandles pdrRecordHandles;
};

} // namespace pldm
