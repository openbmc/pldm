#pragma once

#include <stdint.h>
#include <systemd/sd-bus.h>

#include <sdbusplus/server.hpp>
#include <string>

namespace pldm
{
namespace responder
{

/**
 *  @brief Get the DBUS Service name for the input dbus path
 *  @param[in] bus - DBUS Bus Object
 *  @param[in] path - DBUS object path
 *  @param[in] interface - DBUS Interface
 *  @return std::string - the dbus service name
 */
std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface);

/** @brief Convert any Decimal number to BCD
 *
 *  @tparam[in] decimal - Decimal number
 *  @return Corresponding BCD number
 */
template <typename T>
T decimalToBcd(T decimal)
{
    T bcd = 0;
    T rem = 0;
    auto cnt = 0;

    while (decimal)
    {
        rem = decimal % 10;
        bcd = bcd + (rem << cnt);
        decimal = decimal / 10;
        cnt += 4;
    }

    return bcd;
}

} // namespace responder
} // namespace pldm
