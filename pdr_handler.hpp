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
    RemotePDRHandler() = delete;

    RemotePDRHandler(uint8_t eid, uint8_t tid);

    void fetchPDR(std::vector<uint32_t> recordHandle);

  private:
    std::vector<uint32_t> recordHandle;
    uint8_t mctp_eid;
    uint8_t terminus_id;
    std::vector<std::vector<uint8_t>> pdrs;
};

} // namespace pldm
