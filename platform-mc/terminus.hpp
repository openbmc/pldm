#pragma once

#include "common/types.hpp"
#include "libpldm/platform.h"
#include "numeric_sensor.hpp"

#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/server.hpp>

using namespace pldm::pdr;

namespace pldm
{
namespace platform_mc
{

using InventoryItemBoardIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Board>;

class TerminusManager;

/**
 * @brief Terminus
 *
 * Termuns class keeps the variables for a terminus which is discoved by
 * TerminusManager. It fetchs and parses PDRs for sensor monitoring and control.
 * It also registers xyz.openbmc_project.inventory.Item.Board
 * phosphor-dbus-interface for its sensors associate and be queryable by other
 * service(e.g.,bmcweb).
 */
class Terminus
{
  public:
    Terminus(uint8_t eid, uint8_t tid, sdeventplus::Event& event,
             pldm::requester::Handler<pldm::requester::Request>& handler,
             pldm::dbus_api::Requester& requester, TerminusManager* manager) :
        eid(eid),
        tid(tid), event(event), handler(handler), requester(requester),
        manager(manager)
    {}
    void fetchPDRs();

    uint8_t eid;
    uint8_t tid;
    std::vector<std::shared_ptr<NumericSensor>> numericSensors;

  private:
    void processFetchPDREvent(uint32_t nextRecorHandle,
                              sdeventplus::source::EventBase& source);
    void sendGetPDR(uint32_t record_hndl, uint32_t data_transfer_hndl,
                    uint8_t op_flag, uint16_t request_cnt,
                    uint16_t record_chg_num);
    void handleRespGetPDR(mctp_eid_t _eid, const pldm_msg* response,
                          size_t respMsgLen);
    void parsePDRs();
    std::tuple<TerminusHandle, NumericSensorInfo>
        parseNumericPDR(const std::vector<uint8_t>& pdrData);
    void parseNumericSensorPDRs();
    void addNumericSensor(const NumericSensorInfo& sensorInfo);

    sdeventplus::Event& event;
    pldm::requester::Handler<pldm::requester::Request>& handler;
    pldm::dbus_api::Requester& requester;
    std::vector<std::vector<uint8_t>> numericSensorPDRs{};
    std::vector<std::vector<uint8_t>> entityAuxiliaryNamePDRs{};
    std::unique_ptr<sdeventplus::source::Defer> deferredFetchPDREvent;
    TerminusManager* manager;
    std::shared_ptr<InventoryItemBoardIntf> inventoryItemBoardInft = nullptr;
    std::shared_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;
    std::string inventoryPath;
};
} // namespace platform_mc
} // namespace pldm
