#pragma once

#include <libpldm/base.h>

#include <common/types.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace pldm
{
namespace oem_meta
{

struct MctpEndpoint
{
    uint64_t address;
    uint64_t bus;
    std::string name;
    std::optional<std::string> iana;
    pldm::dbus::ObjectPath objectPath;
};

/**
 * @brief Check the IANA of the MCTP endpoint device that sent the OEM message.
 *
 * @param[in] configurations - Mapping of EID to MCTP endpoint info.
 * @param[in] tid - PLDM TID of the sender.
 * @param[out] bool - True if the endpoint used META IANA, false otherwise.
 */
bool checkMetaIana(const std::map<eid, MctpEndpoint>& configurations,
                   pldm_tid_t tid);

/**
 * @brief Get the slot number from D-Bus for the given sender tid. If
 *        any error or exception occurs during the process, it will
 *        fall back to a hardcoded slot number based on the TID.
 *        This function does not throw exceptions.
 *
 * @param[in] configurations - Mapping of EID to MCTP endpoint info.
 * @param[in] tid - PLDM TID of the sender.
 * @param[out] std::string - Corresponding slot number.
 */
std::string getSlotNumberStringByTID(
    const std::map<eid, MctpEndpoint>& configurations, pldm_tid_t tid);

} // namespace oem_meta
} // namespace pldm
