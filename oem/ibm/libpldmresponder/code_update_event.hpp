#include "libpldm/platform.h"

#include <memory>
#include <vector>

namespace pldm
{

namespace code_update_event
{

using SensorId = uint16_t;

enum codeUpdateStateValues
{
    START = 0x1,
    END = 0x2,
    FAIL = 0x3,
    ABORT = 0x4,
    ACCEPT = 0x5,
    REJECT = 0x6,
};

/** @brief Method to encode code update event msg
 *  @param[in] eventType - type of event
 *  @param[in] eventDataVec - vector of event data to be sent to host
 *  @param[in] instanceId - instance ID
 *  @param[in/out] requestMsg - request msg to be encoded
 *  @return PLDM status code
 */
int encodeEventMsg(uint8_t eventType, const std::vector<uint8_t>& eventDataVec,
                   std::vector<uint8_t>& requestMsg, uint8_t instanceId);

/** @brief Method to send code update event to host
 *  @param[in] mctp_eid - MCTP EID
 *  @param[in] instanceId - instance ID
 *  @param[in] mctp_fd MCTP fd
 *  @param[in] sensorId - sendor ID
 *  @param[in] eventState - new code update event state
 *  @param[in] previousEventState - previous code update event state
 *  @param[in] stateSenserEventPtr - offset of state sensor
 *  @return void
 */
void sendCodeUpdateEvent(uint8_t mctp_eid, uint8_t instanceId, uint8_t mctp_fd,
                         SensorId sensorId, uint8_t eventState,
                         uint8_t previousEventState,
                         uint8_t stateSenserEventPtr);

/** @brief Method to send encoded request msg of code update event to host
 *  @param[in] requestMsg - encoded request msg
 *  @param[in] mctp_fd - MCTP fd
 *  @return PLDM status code
 */
int sendEventToHost(std::vector<uint8_t>& requestMsg, uint8_t mctp_fd);

} // namespace code_update_event
} // namespace pldm
