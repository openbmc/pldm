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

namespace pldm
{

// vector which would hold the PDR record handle data returned by
// pldmPDRRepositoryChgEvent event data
using ChangeEntry = uint32_t;
using PDRRecordHandles = std::vector<ChangeEntry>;

/** @class HostPDRHandler
 *  @brief This class can fetch and process PDRs from host firmware
 *  @details Provides an API to fetch PDRs from the host firmware. Upon
 *  receiving the PDRs, they are stored into the BMC's primary PDR repo.
 *  Adjustments are made to entity association PDRs received from the host,
 *  because they need to be assimilated into the BMC's entity association
 *  tree. A PLDM event containing the record handles of the updated entity
 *  association PDRs is sent to the host.
 */
class HostPDRHandler
{
  public:
    HostPDRHandler() = delete;
    HostPDRHandler(const HostPDRHandler&) = delete;
    HostPDRHandler(HostPDRHandler&&) = delete;
    HostPDRHandler& operator=(const HostPDRHandler&) = delete;
    HostPDRHandler& operator=(HostPDRHandler&&) = delete;
    ~HostPDRHandler() = default;

    /** @brief Constructor
     *  @param[in] mctp_fd - fd of MCTP communications socket
     *  @param[in] mctp_eid - MCTP EID of host firmware
     *  @param[in] event - reference of main event loop of pldmd
     *  @param[in] repo - pointer to BMC's primary PDR repo
     *  @param[in] requester - reference to Requester object
     */
    explicit HostPDRHandler(int mctp_fd, uint8_t mctp_eid,
                            sdeventplus::Event& event, pldm_pdr* repo,
                            Requester& requester) :
        mctp_fd(mctp_fd),
        mctp_eid(mctp_eid), event(event), repo(repo), requester(requester)
    {
    }

    /** @brief fetch PDRs from host firmware. See @class.
     *  @param[in] recordHandles - list of record handles pointing to host's
     *             PDRs that need to be fetched.
     */
    void fetchPDR(std::vector<uint32_t>&& recordHandles);

  private:
    /** @brief fetchPDR schedules work on the event loop, this method does the
     *  actual work. This is so that the PDR exchg with the host is async.
     *  @param[in] source - sdeventplus event source
     */
    void _fetchPDR(sdeventplus::source::EventBase& source);

    /** @brief fd of MCTP communications socket */
    int mctp_fd;
    /** @brief MCTP EID of host firmware */
    uint8_t mctp_eid;
    /** @brief reference of main event loop of pldmd, primarily used to schedule
     *  work.
     */
    sdeventplus::Event& event;
    /** @brief pointer to BMC's primary PDR repo, host PDRs are added here */
    pldm_pdr* repo;
    /** @brief reference to Requester object, primarily used to access API to
     *  obtain PLDM instance id.
     */
    Requester& requester;
    /** @brief sdeventplus event source */
    std::unique_ptr<sdeventplus::source::Defer> pdrFetchEvent;
    /** @brief list of PDR record handles pointing to host's PDRs */
    PDRRecordHandles pdrRecordHandles;
};

} // namespace pldm
