#pragma once

#include <stdint.h>

namespace pldm
{

namespace responder
{

namespace effecter
{

using Id = uint16_t;

Id nextId();

} // namespace effecter
} // namespace responder
} // namespace pldm
