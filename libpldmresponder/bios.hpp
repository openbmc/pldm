#pragma once

#include "libpldmresponder/utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <stdint.h>
#include <systemd/sd-bus.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message/types.hpp>
#include <sdbusplus/server.hpp>
#include <string>
#include <vector>

#include "libpldm/bios.h"

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
