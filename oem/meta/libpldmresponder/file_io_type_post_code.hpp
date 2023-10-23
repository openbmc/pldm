#pragma once

#include "file_io.hpp"

namespace pldm
{
namespace responder
{
namespace oem_meta
{

int postCodeHandler(
    int slot, const std::array<uint8_t, decodeDataMaxLength>& retDataField,
    uint32_t length);

} // namespace oem_meta
} // namespace responder
} // namespace pldm
