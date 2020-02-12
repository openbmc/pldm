#pragma once

#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sdbusplus/server.hpp>
#include <string>
#include <variant>
#include <vector>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include "libpldm/base.h"
#include "libpldm/bios.h"
#include "libpldm/platform.h"

using Json = nlohmann::json;

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
 *  @param[in] effecterCount - the number of individual sets of effecter
 *                              information
 *  @return[out] parse success and get a valid set_effecter_state_field
 *               structure, return nullopt means parse failed
 */
std::optional<std::vector<set_effecter_state_field>>
    parseEffecterData(const std::vector<uint8_t>& effecterData,
                      uint8_t effecterCount);

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

struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
    std::string propertyType; //!< D-Bus property type
};

using PropertyValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string>;

/**
 * @brief The interface for DBusHandler
 */
class DBusHandlerInterface
{
  public:
    virtual ~DBusHandlerInterface() = default;

    virtual void setDbusProperty(const DBusMapping& dBusMap,
                                 const PropertyValue& value) const = 0;
};

/**
 *  @class DBusHandler
 *
 *  Wrapper class to handle the D-Bus calls
 *
 *  This class contains the APIs to handle the D-Bus calls
 *  to cater the request from pldm requester.
 *  A class is created to mock the apis in the test cases
 */
class DBusHandler : public DBusHandlerInterface
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

    /** @brief Set Dbus property
     *
     *  @param[in] dBusMap - Object path, property name, interface and property
     *                       type for the D-Bus object
     *  @param[in] value - The value to be set
     */
    void setDbusProperty(const DBusMapping& dBusMap,
                         const PropertyValue& value) const override;
};

} // namespace utils
} // namespace pldm
