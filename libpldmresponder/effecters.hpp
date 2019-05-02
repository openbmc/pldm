#pragma once

#include <stdint.h>

namespace pldm
{

namespace responder
{

namespace effecter
{

using Id = uint16_t;

/** @brief Get next available id to assign to an effecter
 *
 *  @return  uint16_t - effecter id
 */
Id nextId();

} // namespace effecter
} // namespace responder
} // namespace pldm
