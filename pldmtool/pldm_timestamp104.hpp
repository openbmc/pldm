#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace pldmtool
{

/** @brief Render a DSP0240 v1.2.0 timestamp104 as an ISO 8601 style string.
 *
 *  Fields finer than the encoded time and UTC resolution are omitted. A
 *  two-digit field that is only known to its tens digit keeps that digit
 *  followed by '*', following the CIM DateTime convention the spec refers to.
 */
std::string formatTimestamp104(std::span<const uint8_t> value);

} // namespace pldmtool
