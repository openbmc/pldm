#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"
#include "requester/configuration_discovery_handler.hpp"

#include <libpldm/base.h>

#include <map>
#include <memory>
#include <string>

namespace pldm::oem_meta
{

namespace utils
{

/** @brief Construct unique_ptr to ConfigurationDiscoveryHandler for mctp
 *         endpoint discovery
 */
void initConfigurationDiscoveryHandler(
    const pldm::utils::DBusHandler* dbusHandler);

/** @brief return pointer to ConfigurationDiscoveryHandler for mctp endpoint
 *         discovery
 *
 *  @param[out] pointer to ConfigurationDiscoveryHandler
 */
ConfigurationDiscoveryHandler* getMctpConfigurationHandler();

} // namespace utils

/**
 * @brief Check the IANA of the MCTP endpoint device that sent the OEM message.
 *
 * @param[in] tid - PLDM TID of the sender.
 * @param[out] bool - True if the endpoint used META IANA, false otherwise.
 */
bool checkMetaIana(pldm_tid_t tid);

/**
 * @brief Get the slot number from D-Bus for the given sender tid. 
 *
 *        - If no configurations available, it will return std::string("0").
 *        - If any error or exception occurs during the process, it 
 *          will fall back to a hardcoded slot number based on the TID.
 *
 *        This function does not throw exceptions.
 *
 * @param[in] tid - PLDM TID of the sender.
 * @param[out] std::string - Corresponding slot number.
 */
std::string getSlotNumberStringByTID(pldm_tid_t tid);

} // namespace pldm::oem_meta
