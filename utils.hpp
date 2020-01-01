#pragma once

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <exception>
#include <iostream>
#include <sdbusplus/server.hpp>
#include <string>
#include <variant>
#include <vector>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include "libpldm/base.h"
#include "libpldm/bios.h"
#include "libpldm/platform.h"

namespace pldm
{
namespace utils
{

/** @struct CustomFD
 *
 *  RAII wrapper for file descriptor.
 */
struct CustomFD
{
    CustomFD(const CustomFD&) = delete;
    CustomFD& operator=(const CustomFD&) = delete;
    CustomFD(CustomFD&&) = delete;
    CustomFD& operator=(CustomFD&&) = delete;

    CustomFD(int fd) : fd(fd)
    {
    }

    ~CustomFD()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }

    int operator()() const
    {
        return fd;
    }

  private:
    int fd = -1;
};

/** @brief Calculate the pad for PLDM data
 *
 *  @param[in] data - Length of the data
 *  @return - uint8_t - number of pad bytes
 */
uint8_t getNumPadBytes(uint32_t data);

/** @brief Convert uint64 to date
 *
 *  @param[in] data - time date of uint64
 *  @param[out] year - year number in dec
 *  @param[out] month - month number in dec
 *  @param[out] day - day of the month in dec
 *  @param[out] hour - number of hours in dec
 *  @param[out] min - number of minutes in dec
 *  @param[out] sec - number of seconds in dec
 *  @return true if decode success, false if decode faild
 */
bool uintToDate(uint64_t data, uint16_t* year, uint8_t* month, uint8_t* day,
                uint8_t* hour, uint8_t* min, uint8_t* sec);

/** @brief Convert effecter data to structure of set_effecter_state_field
 *
 *  @param[in] effecterData - the date of effecter
 *  @param[out] effecter_id - a handle that is used to identify and access the
 * effecter
 *  @param[out] stateField - structure of set_effecter_state_field
 */
bool decodeEffecterData(const std::vector<uint8_t>& effecterData,
                        uint16_t& effecter_id,
                        std::vector<set_effecter_state_field>& stateField);

/**
 *  @brief creates an error log
 *  @param[in] errorMsg - the error message
 */
void reportError(const char* errorMsg);

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

constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";

/**
 *  @class DBusHandler
 *
 *  Wrapper class to handle the D-Bus calls
 *
 *  This class contains the APIs to handle the D-Bus calls
 *  to cater the request from pldm requester.
 *  A class is created to mock the apis in the test cases
 */
class DBusHandler
{
  public:
    /** @brief Get the bus connection. */
    static auto& getBus()
    {
        static auto bus = sdbusplus::bus::new_default();
        return bus;
    }

    /**
     *  @brief Get the DBUS Service name for the input dbus path
     *  @param[in] path - DBUS object path
     *  @param[in] interface - DBUS Interface
     *  @return std::string - the dbus service name
     */
    std::string getService(const char* path, const char* interface) const;

    /** @brief API to set a D-Bus property
     *
     *  @param[in] objPath - Object path for the D-Bus object
     *  @param[in] dbusProp - The D-Bus property
     *  @param[in] dbusInterface - The D-Bus interface
     *  @param[in] value - The value to be set
     * failure
     */
    template <typename T>
    void setDbusProperty(const char* objPath, const char* dbusProp,
                         const char* dbusInterface,
                         const std::variant<T>& value) const
    {
        auto& bus = DBusHandler::getBus();
        auto service = getService(objPath, dbusInterface);
        auto method = bus.new_method_call(service.c_str(), objPath,
                                          dbusProperties, "Set");
        method.append(dbusInterface, dbusProp, value);
        bus.call_noreply(method);
    }

    template <typename Variant>
    auto getDbusPropertyVariant(const char* objPath, const char* dbusProp,
                                const char* dbusInterface)
    {
        Variant value;
        auto& bus = DBusHandler::getBus();
        auto service = getService(objPath, dbusInterface);
        auto method = bus.new_method_call(service.c_str(), objPath,
                                          dbusProperties, "Get");
        method.append(dbusInterface, dbusProp);
        auto reply = bus.call(method);
        reply.read(value);

        return value;
    }

    template <typename Property>
    auto getDbusProperty(const char* objPath, const char* dbusProp,
                         const char* dbusInterface)
    {
        auto VariantValue = getDbusPropertyVariant<std::variant<Property>>(
            objPath, dbusProp, dbusInterface);
        return std::get<Property>(VariantValue);
    }
};

} // namespace utils
} // namespace pldm
