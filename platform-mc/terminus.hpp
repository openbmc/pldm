#pragma once

#include "libpldm/platform.h"

#include "common/types.hpp"
#include "requester/handler.hpp"

#include <sdbusplus/server/object.hpp>
#include <sdeventplus/event.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/server.hpp>

using namespace pldm::pdr;

namespace pldm
{
namespace platform_mc
{

using InventoryItemBoardIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Board>;
using SensorId = uint16_t;
using SensorAuxiliaryNames =
    std::tuple<SensorId, uint8_t,
               std::vector<std::pair<std::string, std::string>>>;
class TerminusManager;

/**
 * @brief Terminus
 *
 * The class keeps the variables for a terminus which is discoved by
 * TerminusManager. It fetchs and parses PDRs for sensor monitoring and control.
 * It also registers xyz.openbmc_project.inventory.Item.Board
 * phosphor-dbus-interface for its sensors associate and be queryable by other
 * service.
 */
class Terminus
{
  public:
    Terminus(mctp_eid_t eid, uint8_t tid, sdeventplus::Event& event,
             requester::Handler<requester::Request>& handler,
             dbus_api::Requester& requester, uint64_t& supportedPLDMTypes);
    void parsePDRs();
    void pollForPlatformEventMessage();
    bool doesSupport(uint8_t type);

    mctp_eid_t eid;
    uint8_t tid;
    std::vector<std::vector<uint8_t>> pdrs{};

  private:
    std::shared_ptr<pldm_numeric_sensor_value_pdr>
        parseNumericPDR(std::vector<uint8_t>& pdrData);

    std::tuple<SensorId, uint8_t,
               std::vector<std::pair<std::string, std::string>>>
        parseSensorAuxiliaryNamesPDR(const std::vector<uint8_t>& pdrData);

    sdeventplus::Event& event;
    pldm::requester::Handler<pldm::requester::Request>& handler;
    pldm::dbus_api::Requester& requester;

    std::vector<SensorAuxiliaryNames> sensorAuxiliaryNamesTbl{};
    std::shared_ptr<InventoryItemBoardIntf> inventoryItemBoardInft = nullptr;
    std::string inventoryPath;

    uint64_t supportedTypes;
};
} // namespace platform_mc
} // namespace pldm
