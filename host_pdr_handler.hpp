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

  private:
    std::vector<std::vector<uint8_t>> pdrs;
};

} // namespace pldm
