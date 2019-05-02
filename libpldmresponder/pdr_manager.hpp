#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace pldm
{

namespace responder
{

namespace pdr
{

using Entry = std::vector<uint8_t>;
using PDR = std::vector<Entry>;

void generatePDR(const std::string& dir, PDR& pdrRepo);

} // namespace pdr
} // namespace responder
} // namespace pldm
