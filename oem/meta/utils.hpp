#pragma once

#include <libpldm/base.h>

#include <string>

namespace pldm::oem_meta
{

/**
 * @brief Check the IANA of the MCTP endpoint device that sent the OEM message.
 *
 * @param[in] tid - PLDM TID of the sender.
 * @param[out] bool - True if the endpoint used META IANA, false otherwise.
 */
bool checkMetaIana(const pldm_tid_t& tid);

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
std::string getSlotNumberStringByTID(const pldm_tid_t& tid);

} // namespace pldm::oem_meta
