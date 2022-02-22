#pragma once

#include "libpldm/platform.h"

#include "common/types.hpp"
#include "requester/handler.hpp"

#include <sdbusplus/server/object.hpp>
#include <sdeventplus/event.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/server.hpp>

#include <algorithm>
#include <bitset>
#include <vector>

namespace pldm
{
namespace platform_mc
{

/**
 * @brief Terminus
 *
 * Terminus class holds the TID, supported PLDM Type or PDRs which are needed by
 * other manager class for sensor monitoring and control.
 */
class Terminus
{
  public:
    Terminus(pldm_tid_t tid, uint64_t supportedPLDMTypes);

    /** @brief Check if the terminus supports the PLDM type message
     *
     *  @param[in] type - PLDM Type
     *  @return support state - True if support, otherwise False
     */
    bool doesSupportType(uint8_t type);

    /** @brief Check if the terminus supports the PLDM command message
     *
     *  @param[in] type - PLDM Type
     *  @param[in] command - PLDM command
     *  @return support state - True if support, otherwise False
     */
    bool doesSupportCommand(uint8_t type, uint8_t command);

    /** @brief Set the supported PLDM commands for terminus
     *
     *  @param[in] cmds - bit mask of the supported PLDM commands
     *  @return success state - True if success, otherwise False
     */
    bool setSupportedCommands(const std::vector<uint8_t>& cmds)
    {
        const size_t expectedSize = PLDM_MAX_TYPES *
                                    (PLDM_MAX_CMDS_PER_TYPE / 8);
        if (cmds.empty() || cmds.size() != expectedSize)
        {
            lg2::error(
                "setSupportedCommands received invalid bit mask size. Expected: {EXPECTED}, Received: {RECEIVED}",
                "EXPECTED", expectedSize, "RECEIVED", cmds.size());
            return false;
        }

        /* Assign Vector supportedCmds by Vector cmds */
        supportedCmds.resize(cmds.size());
        std::copy(cmds.begin(), cmds.begin() + cmds.size(),
                  supportedCmds.begin());

        return true;
    }
    /** @brief The getter to return terminus's TID */
    pldm_tid_t getTid()
    {
        return tid;
    }

    /** @brief A list of PDRs fetched from Terminus */
    std::vector<std::vector<uint8_t>> pdrs{};

    /** @brief A flag to indicate if terminus has been initialized */
    bool initialized = false;

  private:
    /* @brief The terminus's TID */
    pldm_tid_t tid;

    /* @brief The supported PLDM command types of the terminus */
    std::bitset<64> supportedTypes;

    /** @brief Store supported PLDM commands of a terminus
     *         Maximum number of PLDM Type is PLDM_MAX_TYPES
     *         Maximum number of PLDM command for each type is
     *         PLDM_MAX_CMDS_PER_TYPE.
     *         Each uint8_t can store the supported state of 8 PLDM commands.
     *         Size of supportedCmds will be
     *         PLDM_MAX_TYPES * (PLDM_MAX_CMDS_PER_TYPE / 8).
     */
    std::vector<uint8_t> supportedCmds;
};
} // namespace platform_mc
} // namespace pldm
