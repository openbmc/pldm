#pragma once

#include "oem/meta/requester/configuration_discovery_handler.hpp"

#include <common/types.hpp>
#include <libpldm/base.h>
#include <libpldm/oem/meta/file_io.h>
#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace pldm::responder::oem_meta
{
using message = std::span<uint8_t>;

[[maybe_unused]] static uint64_t getSlotNumberByTID(const std::map<pldm::dbus::ObjectPath, pldm::oem_meta::MctpEndpoint>& configurations, pldm_tid_t tid) 
{
	static const pldm::dbus::Property slotNumberProperty = "SlotNumber";
	static const pldm::dbus::Interface slotInterface = "xyz.openbmc_project.Inventory.Decorator.Slot";
	static const pldm::dbus::Interfaces slotInterfaces = {slotInterface};

	pldm::utils::GetAncestorsResponse response;

	bool getAncestorsSucc = true;
	for (const auto& [path, mctpEndpoint] : configurations)
	{
			if (mctpEndpoint.EndpointId == tid)
			{
					try
					{
							response = pldm::utils::DBusHandler().getAncestors(
									path.c_str(), slotInterfaces);
					}
					catch (const sdbusplus::exception_t& e)
					{
							error(
									"GetAncestors call failed with error code {ERROR}, path {PATH} and interface {INTERFACE}",
									"ERROR", e, "PATH", path.c_str(), "INTERFACE",
									slotInterface);
							getAncestorsSucc = false;
							break;
					}

					// It should be only one of the slot property existed.
					if (response.size() != 1)
					{
							error("Get invalide number {SIZE} of slot property",
										"SIZE", response.size());
							getAncestorsSucc = false;
							break;
					}
					break;
			}
	}
	if (!getAncestorsSucc){
		throw std::invalid_argument("Configuration of TID not found.");
	}

	bool getSlotNum = true;
	uint64_t slotNum;
	try
	{
			slotNum = pldm::utils::DBusHandler().getDbusProperty<uint64_t>(
					std::get<0>(response.front()).c_str(), slotNumberProperty.c_str(),
					slotInterface.c_str());
	}
	catch (const sdbusplus::exception_t& e)
	{
			error(
					"Error getting Names property with error code {ERROR}, path {PATH} and interface {INTERFACE}",
					"ERROR", e, "PATH", std::get<0>(response.front()).c_str(),
					"INTERFACE", slotInterface);
			getSlotNum = false;
	}
	if (!getSlotNum)
	{
		throw std::invalid_argument("Configuration of TID not found.");
	}

	return slotNum;
}

enum class FileIOType : uint8_t
{
};

/**
 *  @class FileHandler
 *
 *  Base class to handle read/write of all OEM file types
 */
class FileHandler
{
  public:
    /** @brief Method to write data to BMC
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    virtual int write([[maybe_unused]] const message& data)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }

    /** @brief Method to read data from BIC
     *  @param[in] data - eventData
     *  @return  PLDM status code
     */
    virtual int read(
        [[maybe_unused]] struct pldm_oem_meta_file_io_read_resp* data)
    {
        return PLDM_ERROR_UNSUPPORTED_PLDM_CMD;
    }
    virtual ~FileHandler() {}
};

} // namespace pldm::responder::oem_meta
