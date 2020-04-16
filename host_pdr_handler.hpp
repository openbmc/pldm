#pragma once

#include "dbus_impl_requester.hpp"
#include "utils.hpp"

#include <iostream>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/platform.h"

using namespace pldm::dbus_api;
namespace pldm
{
class HostPDRHandler
{
  public:
    // default constructor
    HostPDRHandler() = delete;

    /**@brief fetchPDR based on the recordHandles
     *@param[in] recordHandle - a vector of recordHandles
     *@param[in] mctp_eid - MCTP endpoint id
     *@param[in] fd - MCTP socket fd
     */
    void fetchPDR(const std::vector<uint32_t>& recordHandle, uint8_t mctp_eid,
                  int fd, Requester&);
    /**brief preparepldmPDRRepositoryChgEventData based on the pdrTypes
     *@param[in] pdrTypes - a vector of PDR Types
     *@param[in] eventDataFormat - format of this eventData
     *@param[in] repo - opaque pointer acting as PDR repo handle
     */
    std::vector<uint8_t> preparepldmPDRRepositoryChgEventData(
        const std::vector<uint8_t>& pdrTypes, uint8_t eventDataFormat,
        const pldm_pdr* repo);

    /**brief sendpldmPDRRepositoryChgEventData based on the eventData
     *@param[in] eventData - The event data
     *@param[in] mctp_eid - MCTP endpoint id
     *@param[in] fd - MCTP socket fd
     */
    int sendpldmPDRRepositoryChgEventData(const std::vector<uint8_t> eventData,
                                          uint8_t mctp_eid, int fd, Requester&);

  private:
    std::vector<std::vector<uint8_t>> pdrs;
};

} // namespace pldm
