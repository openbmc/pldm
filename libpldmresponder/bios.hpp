#pragma once

#include <systemd/sd-bus.h>

#include <sdbusplus/server.hpp>

namespace pldm
{

namespace responder
{

/** @brief Handler for GetDateTime
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void getDateTime(const pldm_msg_payload* request, pldm_msg* response);

/**
 * @brief Get the DBUS Service name for the input dbus path
 * @param[in] bus - DBUS Bus Object
 * @param[in] path - DBUS object path
 * @param[in] interface - DBUS Interface
 * **/
std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface);

} // namespace responder
} // namespace pldm
