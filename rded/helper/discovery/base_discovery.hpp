#include "libpldm/pldm_base_requester.h"
#include <iostream>
#include <map>
#include <vector>

#pragma once


extern std::map<uint8_t, int> base_command_request_size;

int perform_base_discovery(std::string rde_device, int fd, int net_id, int eid,
                           int instance_id);