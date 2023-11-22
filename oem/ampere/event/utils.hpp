#include "common/types.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace pldm
{
namespace oem
{

/** @brief Get unique entry ID
 *
 *  @param[in] prefix - prefix name
 *
 *  @return std::string unique entry ID
 */
std::string getUniqueEntryID(std::string& prefix);

/** @brief Log Redfish for FaultLog
 *
 *  @param[in] primaryLogId - unique name
 *  @param[in] type - Crashdump or CPER
 *
 *  @return None
 */
void addFaultLogToRedfish(std::string& primaryLogId, std::string& type);

/** @brief Log OEM SEL for FaultLog
 *
 *  @param[in] msg - message string
 *  @param[in] evtData - event Data
 *  @param[in] recordType - record Type
 *
 *  @return None
 */
void addOEMSelLog(std::string& msg, std::vector<uint8_t>& evtData,
                  uint8_t recordType);

} // namespace oem
} // namespace pldm
