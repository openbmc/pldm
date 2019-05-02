#pragma once

#include <stdint.h>
#include <string>
#include <map>
#include <vector>

#include "libpldm/platform.h"

namespace pldm
{

namespace responder
{

namespace pdr
{

using RecordHandle = uint32_t;
using Entry = std::vector<uint8_t>;

void generatePDR(const std::string& dir, std::map<RecordHandle, Entry>& pdrRepo);

} // namespace pdr
} // namespace responder
} // namespace pldm
