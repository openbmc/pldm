#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

class FirmwareInventoryTest;
class FirmwareInventoryTestInstance;

namespace pldm::fw_update
{

class FirmwareInventory;
using SoftwareVersion = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;
using SoftwareAssociationDefinitions = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

using SoftwareVersionPurpose = SoftwareVersion::VersionPurpose;

using namespace pldm;

class FirmwareInventory
{
  public:
    friend class ::FirmwareInventoryTest;
    friend class ::FirmwareInventoryTestInstance;
    FirmwareInventory() = delete;
    FirmwareInventory(const FirmwareInventory&) = delete;
    FirmwareInventory(FirmwareInventory&&) = delete;
    FirmwareInventory& operator=(const FirmwareInventory&) = delete;
    FirmwareInventory& operator=(FirmwareInventory&&) = delete;

    /**
     * @brief Constructor
     * @param[in] softwareIdentifier - Software identifier containing EID and
     *                                 component identifier
     * @param[in] softwarePath - D-Bus object path for the firmware inventory
     * entry
     * @param[in] softwareVersion - Active version of the firmware
     * @param[in] associatedEndpoint - D-Bus object path of the endpoint
     * associated with the firmware
     * @param[in] descriptors - Descriptors associated with the firmware
     * @param[in] componentInfo - Component information associated with the
     * firmware
     * @param[in] purpose - Purpose of the software version, default is Unknown
     * @note The descriptors and componentInfo parameters are reserved for
     * future use and currently not used in the implementation.
     */
    explicit FirmwareInventory(
        SoftwareIdentifier softwareIdentifier, const std::string& softwarePath,
        const std::string& softwareVersion,
        const std::string& associatedEndpoint,
        SoftwareVersionPurpose purpose = SoftwareVersionPurpose::Unknown);

  private:
    /**
     * @brief Reference to the sdbusplus bus
     */
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();

    /**
     * @brief Software identifier containing EID and component identifier
     */
    SoftwareIdentifier softwareIdentifier;

    /**
     * @brief The D-Bus object path for the firmware inventory entry, obtained
     * by
     */
    std::string softwarePath;

    /**
     * @brief Software association object that represents the associations
     *        for the firmware
     */
    SoftwareAssociationDefinitions association;

    /**
     * @brief Software version object that represents the firmware version
     */
    SoftwareVersion version;
};

} // namespace pldm::fw_update
