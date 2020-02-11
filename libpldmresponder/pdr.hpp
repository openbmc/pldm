#pragma once

#include "libpldmresponder/effecters.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "utils.hpp"

#include "libpldm/platform.h"

namespace fs = std::filesystem;

using namespace pldm::responder::pdrUtils;

using Type = uint8_t;
using generateHandler = std::function<void(const Json& json, Repo& repo)>;

namespace pldm
{

namespace responder
{

namespace pdr
{
/** @brief Parse PDR JSONs and build PDR repository
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *  @param[in] repo - instance of concrete implementation of Repo
 */
void generate(const std::string& dir, Repo& repo);

/** @brief Build (if not built already) and retrieve PDR
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *
 *  @return Repo& - Reference to instance of pdr::Repo
 */
Repo& getRepo(const std::string& dir);

/** @brief Build (if not built already) and retrieve PDR by the PDR types
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *  @param[in] pdr_type - the different types of PDRs
 *
 *  @return Repo - Instance of pdr::Repo
 */
Repo getRepoByType(const std::string& dir, const Type pdr_type);

/** @brief Get the record of PDR by the record handle
 *
 *  @param[in] pdrRepo - pdr::Repo
 *  @param[in] record_handle - The recordHandle value for the PDR to be
 * retrieved.
 *  @param[out] pdrEntry - Pointer to PDR entry structure
 *
 *  @return pldm_pdr_record - Instance of pdr::Repo
 */
const pldm_pdr_record* getRecordByHandle(Repo& pdrRepo,
                                         const RecordHandle record_handle,
                                         PdrEntry& pdrEntry);

} // namespace pdr
} // namespace responder
} // namespace pldm
