#include "libpldm/pldm_base_requester.h"
#include <iostream>
#include <map>
#include <vector>

#pragma once


extern std::map<uint8_t, int> baseCommandRequestSize;

int performBaseDiscovery(std::string rdeDevice, int fd, int netId, int eid,
                           int instanceId);
