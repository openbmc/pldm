#pragma once

#include "utils.hpp"

#include <iostream>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/platform.h"

namespace pldm
{

class RemotePDRHandler
{
  public:
    // default constructor
    RemotePDRHandler() = delete;

    /**@brief constructor for RemotePDRHandler
     *@param[in] eid - MCTP eid
     *@param[in] tid - PLDM Terminus Id
     */
    RemotePDRHandler(uint8_t eid, uint8_t tid);

    /**@brief fetchPDR based on the recordHandles
     *@param[in] recordHandle - a vector of recordHandles
     */
    void fetchPDR(const std::vector<uint32_t>& recordHandle);

  private:
    uint8_t mctp_eid;
    uint8_t terminus_id;
    std::vector<std::vector<uint8_t>> pdrs;
};

} // namespace pldm
