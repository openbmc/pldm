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
using SensorId = uint16_t;
using SensorAuxiliaryNames =
    std::tuple<SensorId, uint8_t,
               std::vector<std::pair<std::string, std::string>>>;

/**
 * @brief Terminus
 *
 * The Terminus class provides APIs keeps data the  which is needed for sensor
 * monitoring and control.
 */
class Terminus
{
  public:
    Terminus(mctp_eid_t _eid, uint8_t _tid, uint64_t supportedPLDMTypes);
    bool doesSupport(uint8_t type);
    void parsePDRs();

    mctp_eid_t eid()
    {
        return _eid;
    }

    uint8_t tid()
    {
        return _tid;
    }

    std::vector<std::vector<uint8_t>> pdrs{};

  private:
    std::shared_ptr<pldm_numeric_sensor_value_pdr>
        parseNumericPDR(std::vector<uint8_t>& pdrData);

    std::shared_ptr<SensorAuxiliaryNames>
        parseSensorAuxiliaryNamesPDR(const std::vector<uint8_t>& pdrData);

    mctp_eid_t _eid;
    uint8_t _tid;
    uint64_t supportedTypes;

    std::vector<std::shared_ptr<SensorAuxiliaryNames>>
        sensorAuxiliaryNamesTbl{};
};
} // namespace platform_mc
} // namespace pldm
