#pragma once

#include "dbus_impl_requester.hpp"
#include "utils.hpp"

#include <sdeventplus/event.hpp>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/platform.h"

using namespace pldm::dbus_api;

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
    void fetchPDR(const std::vector<uint32_t>& recordHandles);

  private:
    int mctp_fd;
    uint8_t mctp_eid;
    sdeventplus::Event& event;
    pldm_pdr* repo;
    Requester& requester;
};

} // namespace pldm
