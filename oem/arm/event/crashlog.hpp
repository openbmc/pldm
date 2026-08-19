#pragma once

#include <libpldm/base.h>

#include <cstdint>

namespace pldm
{
namespace platform_mc
{
class Manager;
}

namespace oem_arm
{
namespace crashlog
{

/** @brief Check whether a state sensor can report crashlog file updates.
 *
 * @param[in] sensorId PLDM state sensor ID from the event.
 * @return true when this sensor is part of the crashlog file-ready set.
 */
bool isFileStateSensor(uint16_t sensorId);

/** @brief Process a Device File state sensor event for crashlog collection.
 *
 * Valid crashlog file-ready events are coalesced so multiple file instance
 * notifications create one archive and one System Dump entry.
 *
 * @param[in] manager Platform manager containing discovered file descriptors.
 * @param[in] tid Terminus ID that emitted the state sensor event.
 * @param[in] sensorId State sensor ID from the event.
 * @param[in] eventState Decoded Device File state value.
 * @return PLDM completion code.
 */
int processFileStateEvent(platform_mc::Manager* manager, pldm_tid_t tid,
                          uint16_t sensorId, uint8_t eventState);

} // namespace crashlog
} // namespace oem_arm
} // namespace pldm
