#pragma once

#include "libpldm/platform.h"

#include "common/types.hpp"
#include "numeric_sensor.hpp"

#include <sdbusplus/server/object.hpp>
#include <sdeventplus/event.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/server.hpp>

using namespace pldm::pdr;

namespace pldm
{
namespace platform_mc
{
using SensorId = uint16_t;
using SensorAuxiliaryNames =
    std::tuple<SensorId, uint8_t,
               std::vector<std::pair<std::string, std::string>>>;
using InventoryItemBoardIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Board>;
class TerminusManager;

/**
 * @brief Terminus
 *
 * Terminus class holds the EID, TID, supportedPLDMType or PDRs which are
 * needed by other manager class for sensor monitoring and control.
 */
class Terminus
{
  public:
    Terminus(mctp_eid_t _eid, uint8_t _tid, uint64_t supportedPLDMTypes);

    /** @brief Check if the terminus supports the PLDM type message
     *
     *  @param[in] type - PLDM Type
     */
    bool doesSupport(uint8_t type);

    /** @brief Parse the PDRs stored in the member variable, pdrs. */
    void parsePDRs();
    void pollForPlatformEventMessage();

    /** @brief The getter to return EID */
    mctp_eid_t eid()
    {
        return _eid;
    }

    /** @brief The getter to return TID */
    uint8_t tid()
    {
        return _tid;
    }

    /** @brief A list of PDRs fetched from Terminus */
    std::vector<std::vector<uint8_t>> pdrs{};
    std::vector<std::shared_ptr<NumericSensor>> numericSensors{};

  private:
    std::shared_ptr<pldm_numeric_sensor_value_pdr>
        parseNumericPDR(std::vector<uint8_t>& pdrData);

    std::shared_ptr<SensorAuxiliaryNames>
        parseSensorAuxiliaryNamesPDR(const std::vector<uint8_t>& pdrData);

    void addNumericSensor(
        const std::shared_ptr<pldm_numeric_sensor_value_pdr> pdr);

    mctp_eid_t _eid;
    uint8_t _tid;
    uint64_t supportedTypes;

    std::vector<std::shared_ptr<SensorAuxiliaryNames>>
        sensorAuxiliaryNamesTbl{};

    std::shared_ptr<InventoryItemBoardIntf> inventoryItemBoardInft = nullptr;
    std::string inventoryPath;
};
} // namespace platform_mc
} // namespace pldm
