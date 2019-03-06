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
 */
std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface);

/** @brief Convert any Decimal number to BCD
 *
 *  @param[in] decimal - Decimal number of any digits
 *  @param[out] bcd - Corresponding BCD number
 */
template <typename TYPE>
void decimalToBcd(TYPE decimal, TYPE& bcd)
{
    bcd = 0;
    TYPE rem = 0;
    auto cnt = 0;

    while (decimal)
    {
        rem = decimal % 10;
        bcd = bcd + (rem << cnt);
        decimal = decimal / 10;
        cnt += 4;
    }
}

} // namespace responder
} // namespace pldm
